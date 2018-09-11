/*
 *
 * Copyright (c) 2018 The University of Waikato, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This file is part of OpenLI.
 *
 * This code has been developed by the University of Waikato WAND
 * research group. For further information please see http://www.wand.net.nz/
 *
 * OpenLI is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenLI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libtrace.h>
#include <libwandder.h>
#include <libwandder_etsili.h>

#include "logger.h"
#include "collector.h"
#include "intercept.h"
#include "collector_export.h"
#include "etsili_core.h"
#include "ipiri.h"
#include "internetaccess.h"

static void free_ipiri_parameters(etsili_generic_t *params,
        etsili_generic_t **freegenerics) {

    etsili_generic_t *oldp, *tmp;

    HASH_ITER(hh, params, oldp, tmp) {
        HASH_DELETE(hh, params, oldp);
        if (oldp->itemnum == IPIRI_CONTENTS_POP_IDENTIFIER) {
            ipiri_free_id((ipiri_id_t *)(oldp->itemptr));
        }
        release_etsili_generic(freegenerics, oldp);
    }

}

int sort_generics(etsili_generic_t *a, etsili_generic_t *b) {
    if (a->itemnum < b->itemnum) {
        return -1;
    }
    if (a->itemnum > b->itemnum) {
        return 1;
    }

    return 0;
}


int encode_ipiri(etsili_generic_t **freegenerics,
        wandder_encoder_t **encoder, shared_global_info_t *shared,
        openli_ipiri_job_t *job,
        exporter_intercept_msg_t *intdetails, uint32_t seqno,
        openli_exportmsg_t *msg, int iteration) {

    wandder_etsipshdr_data_t hdrdata;
    etsili_generic_t *np, *params = NULL;
    etsili_iri_type_t iritype;
    etsili_ipaddress_t targetip;
    int64_t ipversion = 0;
    struct timeval tv;
    int ret = 0;
#if 0

    /* Conventional IRIs will have an attached plugin which knows how
     * to convert the current "IP session state" into sensible IRI
     * parameters.
     *
     * Unconventional IRIs, which are generated by changes
     * to OpenLI configuration rather than an observed change in IP session
     * state, require us to manually fill in the event type parameter etc.
     */
    if (job->plugin) {
        if ((ret = job->plugin->generate_iri_data(job->plugin, job->plugin_data,
                &params, &iritype, freegenerics, iteration)) < 0) {
            return -1;
        }
    }
#endif

    if (job->special == OPENLI_IPIRI_ENDWHILEACTIVE) {
        uint32_t evtype = IPIRI_END_WHILE_ACTIVE;
        iritype = ETSILI_IRI_REPORT;

        np = create_etsili_generic(freegenerics,
                IPIRI_CONTENTS_ACCESS_EVENT_TYPE, sizeof(uint32_t),
                (uint8_t *)(&evtype));
        HASH_ADD_KEYPTR(hh, params, &(np->itemnum), sizeof(np->itemnum),
                np);
    } else if (job->special == OPENLI_IPIRI_STARTWHILEACTIVE) {
        uint32_t evtype = IPIRI_START_WHILE_ACTIVE;
        iritype = ETSILI_IRI_BEGIN;

        np = create_etsili_generic(freegenerics,
                IPIRI_CONTENTS_ACCESS_EVENT_TYPE, sizeof(uint32_t),
                (uint8_t *)(&evtype));
        HASH_ADD_KEYPTR(hh, params, &(np->itemnum), sizeof(np->itemnum),
                np);
    } else if (job->special == OPENLI_IPIRI_SILENTLOGOFF) {
        uint32_t evtype = IPIRI_ACCESS_END;     // unsure if correct?
        iritype = ETSILI_IRI_END;

        np = create_etsili_generic(freegenerics,
                IPIRI_CONTENTS_ACCESS_EVENT_TYPE, sizeof(uint32_t),
                (uint8_t *)(&evtype));
        HASH_ADD_KEYPTR(hh, params, &(np->itemnum), sizeof(np->itemnum),
                np);

        /* TODO probably need to set an endReason in here, but not sure
         * what is the right reason to use.
         */
    }


    np = create_etsili_generic(freegenerics,
            IPIRI_CONTENTS_INTERNET_ACCESS_TYPE, sizeof(uint32_t),
            (uint8_t *)&(job->access_tech));
    HASH_ADD_KEYPTR(hh, params, &(np->itemnum), sizeof(np->itemnum), np);

    if (job->username) {
        np = create_etsili_generic(freegenerics,
                IPIRI_CONTENTS_TARGET_USERNAME, strlen(job->username),
                job->username);
        HASH_ADD_KEYPTR(hh, params, &(np->itemnum), sizeof(np->itemnum),
                np);
    }

    if (job->ipfamily != 0) {
        uint8_t etsiipmethod = ETSILI_IPADDRESS_ASSIGNED_UNKNOWN;

        switch(job->ipassignmentmethod) {
            case OPENLI_IPIRI_IPMETHOD_UNKNOWN:
                etsiipmethod = ETSILI_IPADDRESS_ASSIGNED_UNKNOWN;
                break;
            case OPENLI_IPIRI_IPMETHOD_STATIC:
                etsiipmethod = ETSILI_IPADDRESS_ASSIGNED_STATIC;
                break;
            case OPENLI_IPIRI_IPMETHOD_DYNAMIC:
                etsiipmethod = ETSILI_IPADDRESS_ASSIGNED_DYNAMIC;
                break;
        }

        if (job->ipfamily == AF_INET) {
            struct sockaddr_in *in = (struct sockaddr_in *)&(job->assignedip);
            etsili_create_ipaddress_v4(
                    (uint32_t *)(&(in->sin_addr.s_addr)),
                    job->assignedip_prefixbits,
                    etsiipmethod, &targetip);
            ipversion = IPIRI_IPVERSION_4;
        } else if (job->ipfamily == AF_INET6) {
            struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)
                    &(job->assignedip);
            etsili_create_ipaddress_v6(
                    (uint8_t *)(&(in6->sin6_addr.s6_addr)),
                    job->assignedip_prefixbits,
                    etsiipmethod, &targetip);
            ipversion = IPIRI_IPVERSION_6;
        }

        if (ipversion == IPIRI_IPVERSION_4 || ipversion == IPIRI_IPVERSION_6) {
            np = create_etsili_generic(freegenerics,
                    IPIRI_CONTENTS_IPVERSION, sizeof(int64_t),
                    (uint8_t *)(&ipversion));
            HASH_ADD_KEYPTR(hh, params, &(np->itemnum), sizeof(np->itemnum),
                    np);

            np = create_etsili_generic(freegenerics,
                    IPIRI_CONTENTS_TARGET_IPADDRESS,
                    sizeof(etsili_ipaddress_t), (uint8_t *)(&targetip));
            HASH_ADD_KEYPTR(hh, params, &(np->itemnum), sizeof(np->itemnum),
                    np);
        }
    }

    if (job->sessionstartts.tv_sec > 0) {
        np = create_etsili_generic(freegenerics,
                IPIRI_CONTENTS_STARTTIME,
                sizeof(struct timeval), (uint8_t *)&(job->sessionstartts));
        HASH_ADD_KEYPTR(hh, params, &(np->itemnum), sizeof(np->itemnum),
                np);
    }

    HASH_SORT(params, sort_generics);


    if (*encoder == NULL) {
        *encoder = init_wandder_encoder();
    } else {
        reset_wandder_encoder(*encoder);
    }

    gettimeofday(&tv, NULL);
    hdrdata.liid = intdetails->liid;
    hdrdata.liid_len = intdetails->liid_len;
    hdrdata.authcc = intdetails->authcc;
    hdrdata.authcc_len = intdetails->authcc_len;
    hdrdata.delivcc = intdetails->delivcc;
    hdrdata.delivcc_len = intdetails->delivcc_len;
    hdrdata.operatorid = shared->operatorid;
    hdrdata.operatorid_len = shared->operatorid_len;
    hdrdata.networkelemid = shared->networkelemid;
    hdrdata.networkelemid_len = shared->networkelemid_len;
    hdrdata.intpointid = shared->intpointid;
    hdrdata.intpointid_len = shared->intpointid_len;

    memset(msg, 0, sizeof(openli_exportmsg_t));
    msg->msgbody = encode_etsi_ipiri(*encoder, &hdrdata,
            (int64_t)(job->cin), (int64_t)seqno, iritype, &tv, params);
    msg->liid = intdetails->liid;
    msg->liidlen = intdetails->liid_len;

    msg->encoder = *encoder;
    msg->ipcontents = NULL;
    msg->ipclen = 0;
    msg->header = construct_netcomm_protocol_header(
            msg->msgbody->len + msg->liidlen + sizeof(msg->liidlen),
            OPENLI_PROTO_ETSI_IRI, 0, &(msg->hdrlen));

    free_ipiri_parameters(params, freegenerics);
    return ret;
}

int ipiri_create_id_printable(char *idstr, int length, ipiri_id_t *iriid) {

    if (length <= 0) {
        return -1;
    }

    if (length > 128) {
        logger(LOG_INFO, "OpenLI: Printable IPIRI ID is too long, truncating to 128 characters.");
        length = 128;
    }

    iriid->type = IPIRI_ID_PRINTABLE;
    iriid->content.printable = (char *)malloc(length + 1);
    memcpy(iriid->content.printable, idstr, length);

    if (iriid->content.printable[length - 1] != '\0') {
        iriid->content.printable[length] = '\0';
    }
    return 0;
}

int ipiri_create_id_mac(uint8_t *macaddr, ipiri_id_t *iriid) {
    /* TODO */
    return -1;
}

int ipiri_create_id_ipv4(uint32_t addrnum, uint8_t slashbits,
        ipiri_id_t *iriid) {
    /* TODO */
    return -1;
}

void ipiri_free_id(ipiri_id_t *iriid) {
    if (iriid->type == IPIRI_ID_PRINTABLE) {
        free(iriid->content.printable);
    }
}

// vim: set sw=4 tabstop=4 softtabstop=4 expandtab :
