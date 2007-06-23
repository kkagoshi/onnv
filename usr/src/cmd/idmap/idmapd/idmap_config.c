/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Config routines common to idmap(1M) and idmapd(1M)
 */

#include <stdlib.h>
#include <synch.h>
#include <assert.h>
#include <sys/varargs.h>
#include <sys/systeminfo.h>
#include <strings.h>
#include <libintl.h>
#include <ctype.h>
#include <errno.h>
#include "idmap_config.h"
#include <stdio.h>
#include <stdarg.h>

#define	FMRI_BASE "svc:/system/idmap"

#define	CONFIG_PG "config"
#define	GENERAL_PG "general"

#define	IDMAP_CFG_DEBUG 0

/* initial length of the array for policy options/attributes: */
#define	DEF_ARRAY_LENGTH 16

static char errmess_buf [1000] =
	"Internal error: idmap configuration has not been initialized";

static void
errmess(char *format, va_list ap)
{
/*LINTED: E_SEC_PRINTF_VAR_FMT*/
	(void) vsnprintf(errmess_buf, sizeof (errmess_buf), format, ap);
	(void) strlcat(errmess_buf, "\n", sizeof (errmess_buf));

#if IDMAP_CFG_DEBUG
	(void) fprintf(stderr, errmess_buf);
	fflush(stderr);
#endif
}


static void
idmap_error(char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	errmess(format, ap);
	va_end(ap);
}

static void
idmap_scf_error(char *format, ...)
{
	const char *scf_message;
	char *new_format;
	char *sep = ": ";
	va_list ap;

	va_start(ap, format);

	scf_message = scf_strerror(scf_error());
	new_format = (char *) malloc(sizeof (char) *
	    (strlen(format) + strlen(scf_message) + strlen(sep) + 1));

	(void) strcpy(new_format, format);
	(void) strcat(new_format, sep);
	(void) strcat(new_format, scf_message);

	errmess(new_format, ap);

	va_end(ap);
	free(new_format);
}

char *
idmap_cfg_error() {
	return (errmess_buf);
}

/* Check if in the case of failure the original value of *val is preserved */
static int
get_val_int(idmap_cfg_t *cfg, char *name, void *val, scf_type_t type)
{
	int rc = 0;

	scf_property_t *scf_prop = scf_property_create(cfg->handles.main);
	scf_value_t *value = scf_value_create(cfg->handles.main);


	if (0 > scf_pg_get_property(cfg->handles.config_pg, name, scf_prop))
	/* this is OK: the property is just undefined */
		goto destruction;


	if (0 > scf_property_get_value(scf_prop, value))
	/* It is still OK when a property doesn't have any value */
		goto destruction;

	switch (type) {
	case SCF_TYPE_BOOLEAN:
		rc = scf_value_get_boolean(value, val);
		break;
	case SCF_TYPE_COUNT:
		rc = scf_value_get_count(value, val);
		break;
	case SCF_TYPE_INTEGER:
		rc = scf_value_get_integer(value, val);
		break;
	default:
		idmap_scf_error(gettext("Internal error: invalid int type %d"),
		    type);
		rc = -1;
		break;
	}


destruction:
	scf_value_destroy(value);
	scf_property_destroy(scf_prop);

	return (rc);
}

static char *
scf_value2string(scf_value_t *value) {
	int rc = -1;
	char buf_size = 127;
	int length;
	char *buf = NULL;
	buf = (char *) malloc(sizeof (char) * buf_size);

	for (;;) {
		length = scf_value_get_astring(value, buf, buf_size);
		if (length < 0) {
			rc = -1;
			goto destruction;
		}

		if (length == buf_size - 1) {
			buf_size *= 2;
			buf = (char *)realloc(buf, buf_size * sizeof (char));
			if (!buf) {
				idmap_scf_error(
					gettext("Not enough memory"));
				rc = -1;
				goto destruction;
			}
		} else {
			rc = 0;
			break;
		}
	}

destruction:
	if (rc < 0) {
		if (buf)
			free(buf);
		buf = NULL;
	}

	return (buf);
}


static int
get_val_astring(idmap_cfg_t *cfg, char *name, char **val)
{
	int rc = 0;

	scf_property_t *scf_prop = scf_property_create(cfg->handles.main);
	scf_value_t *value = scf_value_create(cfg->handles.main);


	if (0 > scf_pg_get_property(cfg->handles.config_pg, name, scf_prop))
	/* this is OK: the property is just undefined */
		goto destruction;

	if (0 > scf_property_get_value(scf_prop, value)) {
		idmap_scf_error(gettext("Cannot get the astring %s"), name);
		rc = -1;
		goto destruction;
	}

	if (!(*val = scf_value2string(value))) {
		rc = -1;
		idmap_scf_error(gettext("Cannot retrieve the astring %s"),
		    name);
	}

destruction:
	scf_value_destroy(value);
	scf_property_destroy(scf_prop);

	if (rc < 0) {
		if (*val)
			free(*val);
		*val = NULL;
	}

	return (rc);
}

int
idmap_cfg_load(idmap_cfg_t *cfg)
{
	int rc = 0;

	cfg->pgcfg.list_size_limit = 0;
	cfg->pgcfg.mapping_domain = NULL;
	cfg->pgcfg.machine_sid = NULL;
	cfg->pgcfg.domain_controller = NULL;
	cfg->pgcfg.global_catalog = NULL;

	if (0 > scf_pg_update(cfg->handles.config_pg)) {
		idmap_scf_error(gettext("Error updating config pg"));
		return (-1);
	}

	if (0 > scf_pg_update(cfg->handles.general_pg)) {
		idmap_scf_error(gettext("Error updating general pg"));
		return (-1);
	}

	rc = get_val_int(cfg, "list_size_limit",
	    &cfg->pgcfg.list_size_limit, SCF_TYPE_COUNT);
	if (rc != 0)
		return (-1);

	rc = get_val_astring(cfg, "mapping_domain",
	    &cfg->pgcfg.mapping_domain);
	if (rc != 0)
		return (-1);

	/*
	 * TBD:
	 * If there is no mapping_domain in idmap's smf config then
	 * set it to the joined domain.
	 * Till domain join is implemented, temporarily set it to
	 * the system domain for testing purposes.
	 */
	if (!cfg->pgcfg.mapping_domain) 	{
		char test[1];
		long dname_size = sysinfo(SI_SRPC_DOMAIN, test, 1);
		if (dname_size > 0) {
			cfg->pgcfg.mapping_domain =
			    (char *)malloc(dname_size * sizeof (char));
			dname_size = sysinfo(SI_SRPC_DOMAIN,
			    cfg->pgcfg.mapping_domain, dname_size);
		}
		if (dname_size <= 0) {
			idmap_scf_error(
			    gettext("Error obtaining the default domain"));
			if (cfg->pgcfg.mapping_domain)
				free(cfg->pgcfg.mapping_domain);
			cfg->pgcfg.mapping_domain = NULL;
		}
	}

	rc = get_val_astring(cfg, "machine_sid", &cfg->pgcfg.machine_sid);
	if (rc != 0)
		return (-1);

	rc = get_val_astring(cfg, "global_catalog", &cfg->pgcfg.global_catalog);
	if (rc != 0)
		return (-1);

	rc = get_val_astring(cfg, "domain_controller",
	    &cfg->pgcfg.domain_controller);
	if (rc != 0)
		return (-1);

	return (rc);
}

idmap_cfg_t *
idmap_cfg_init() {
	/*
	 * The following initializes 'cfg'.
	 */

	/* First the smf repository handles: */
	idmap_cfg_t *cfg = calloc(1, sizeof (idmap_cfg_t));
	if (!cfg) {
		idmap_error(gettext("Not enough memory"));
		return (NULL);
	}

	if (!(cfg->handles.main = scf_handle_create(SCF_VERSION))) {
		idmap_scf_error(gettext("SCF handle not created"));
		goto error;
	}

	if (0 > scf_handle_bind(cfg->handles.main)) {
		idmap_scf_error(gettext("SCF connection failed"));
		goto error;
	}

	if (!(cfg->handles.service = scf_service_create(cfg->handles.main)) ||
	    !(cfg->handles.instance = scf_instance_create(cfg->handles.main)) ||
	    !(cfg->handles.config_pg = scf_pg_create(cfg->handles.main)) ||
	    !(cfg->handles.general_pg = scf_pg_create(cfg->handles.main))) {
		idmap_scf_error(gettext("SCF handle creation failed"));
		goto error;
	}

	if (0 > scf_handle_decode_fmri(cfg->handles.main,
		FMRI_BASE "/:properties/" CONFIG_PG,
		NULL,				/* scope */
		cfg->handles.service,		/* service */
		cfg->handles.instance,		/* instance */
		cfg->handles.config_pg,		/* pg */
		NULL,				/* prop */
		SCF_DECODE_FMRI_EXACT)) {
		idmap_scf_error(gettext("SCF fmri decoding failed"));
		goto error;

	}

	if (0 > scf_service_get_pg(cfg->handles.service,
		GENERAL_PG, cfg->handles.general_pg)) {
		idmap_scf_error(gettext("SCF general pg not obtained"));
		goto error;
	}

	return (cfg);

error:
	(void) idmap_cfg_fini(cfg);
	return (NULL);
}

/* ARGSUSED */
static void
idmap_pgcfg_fini(idmap_pg_config_t *pgcfg) {
	if (pgcfg->mapping_domain)
		free(pgcfg->mapping_domain);
	if (pgcfg->machine_sid)
		free(pgcfg->mapping_domain);
	if (pgcfg->global_catalog)
		free(pgcfg->global_catalog);
	if (pgcfg->domain_controller)
		free(pgcfg->domain_controller);
}

int
idmap_cfg_fini(idmap_cfg_t *cfg)
{
	idmap_pgcfg_fini(&cfg->pgcfg);

	scf_pg_destroy(cfg->handles.config_pg);
	scf_pg_destroy(cfg->handles.general_pg);
	scf_instance_destroy(cfg->handles.instance);
	scf_service_destroy(cfg->handles.service);
	scf_handle_destroy(cfg->handles.main);
	free(cfg);

	return (0);
}