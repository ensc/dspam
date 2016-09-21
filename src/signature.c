#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "libdspam_objects.h"

#include <stdlib.h>
#include <errno.h>

#include "error.h"

struct _ds_spam_signature *_ds_spam_signature_new(size_t len)
{
	struct _ds_spam_signature	*res;
	void				*data;

	res = malloc(sizeof *res);
	if (!res) {
		LOG(LOG_CRIT, "malloc(<spam_signature>): %s", strerror(errno));
		return NULL;
	}

	if (len == 0) {
		data = NULL;
	} else {
		data = malloc(len);
		if (!data) {
			LOG(LOG_CRIT, "malloc(<spam_sigdata/%zu>): %s",
			    len, strerror(errno));
			free(res);
			return NULL;
		}
	}

	*res = (struct _ds_spam_signature) {
		.data		= data,
		.length		= len,
		.ref_cnt	= 1,
	};

	return res;
}

struct _ds_spam_signature *_ds_spam_signature_get(struct _ds_spam_signature *sig)
{
	if (!sig)
		return NULL;

	if (sig->ref_cnt == 0) {
		LOG(LOG_CRIT, "internal error: referencing deleted signature");
		return NULL;
	}

	++sig->ref_cnt;
	return sig;
}

void _ds_spam_signature_put(struct _ds_spam_signature *sig)
{
	if (!sig)
		return;

	switch (sig->ref_cnt) {
	case 0:
		LOG(LOG_CRIT,
		    "internal error: trying to delete dereferenced signature");
		break;

	case 1:
		free(sig->data);
		free(sig);
		break;

	default:
		--sig->ref_cnt;
		break;
	}
}
