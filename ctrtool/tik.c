#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "aes_keygen.h"
#include "tik.h"
#include "ctr.h"
#include "utils.h"

void tik_init(tik_context* ctx)
{
	memset(ctx, 0, sizeof(tik_context));
}

void tik_set_file(tik_context* ctx, FILE* file)
{
	ctx->file = file;
}

void tik_set_offset(tik_context* ctx, u64 offset)
{
	ctx->offset = offset;
}

void tik_set_size(tik_context* ctx, u32 size)
{
	ctx->size = size;
}

void tik_set_usersettings(tik_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}

const unsigned char* tik_get_titlekey(tik_context* ctx)
{
	return ctx->titlekey.valid ? ctx->titlekey.data : NULL;
}

void tik_get_titleid(tik_context* ctx, u8 titleid[8])
{
	memcpy(titleid, ctx->tik.title_id, 8);
}

void tik_get_iv(tik_context* ctx, u8 iv[16])
{
	memset(iv, 0, 16);
	memcpy(iv, ctx->tik.title_id, 8);
}

int tik_decrypt_titlekey(tik_context* ctx, u8 decryptedkey[0x10]) 
{
	u8 iv[16];
	u8* commonkey = settings_get_common_key(ctx->usersettings, ctx->tik.commonkey_idx);

	memset(decryptedkey, 0, 0x10);
	if (!commonkey)
	{
		fprintf(stdout, "Error, could not read common key.\n");
		return 1;
	}
	
	memset(iv, 0, 0x10);
	memcpy(iv, ctx->tik.title_id, 8);

	ctr_init_cbc_decrypt(&ctx->aes, commonkey, iv);
	ctr_decrypt_cbc(&ctx->aes, ctx->tik.encrypted_title_key, decryptedkey, 0x10);
	return 0;
}

void tik_process(tik_context* ctx, u32 actions)
{
	if (ctx->size < sizeof(eticket))
	{
		fprintf(stderr, "Error, ticket size too small\n");
		goto clean;
	}

	fseeko64(ctx->file, ctx->offset, SEEK_SET);
	fread((u8*)&ctx->tik, 1, sizeof(eticket), ctx->file);

	ctx->titlekey.valid = tik_decrypt_titlekey(ctx, ctx->titlekey.data) == 0 ? 1 : 0;

	if (actions & InfoFlag)
	{
		tik_print(ctx); 
	}

clean:
	return;
}

void tik_print(tik_context* ctx)
{
	int i;
	eticket* tik = &ctx->tik;

	fprintf(stdout, "\nTicket content:\n");
	fprintf(stdout,
		"Signature Type:         %08x\n"
		"Issuer:                 %s\n",
		getle32(tik->sig_type), tik->issuer
	);

	fprintf(stdout, "Signature:\n");
	hexdump(tik->signature, 0x100);
	fprintf(stdout, "\n");

	memdump(stdout, "Encrypted Titlekey:     ", tik->encrypted_title_key, 0x10);
	
	if (ctx->titlekey.valid)
		memdump(stdout, "Decrypted Titlekey:     ", ctx->titlekey.data, 0x10);

	memdump(stdout,	"Ticket ID:              ", tik->ticket_id, 0x08);
	fprintf(stdout, "Ticket Version:         %d\n", getle16(tik->ticket_version));
	memdump(stdout,	"Title ID:               ", tik->title_id, 0x08);
	fprintf(stdout, "Common Key Index:       %d\n", tik->commonkey_idx);

	fprintf(stdout, "Content permission map:\n");
	for(i = 0; i < 0x40; i++) {
		printf(" %02x", tik->content_permissions[i]);

		if ((i+1) % 8 == 0)
			printf("\n");
	}
	printf("\n");
}
