/* Copyright (c) 2018 Mozilla */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include "arch.h"
#include "lpcnet.h"
#include "freq.h"

#define MODE_ENCODE 0
#define MODE_DECODE 1
#define MODE_FEATURES 2
#define MODE_SYNTHESIS 3
#define MODE_PLC 4

int main(int argc, char **argv) {
    int mode;
    int plc_percent=0;
    FILE *fin, *fout;
    FILE *plc_file = NULL;
    if (argc != 4 && !(argc == 5 && (strcmp(argv[1], "-plc") == 0 || strcmp(argv[1], "-plc_file") == 0)))
    {
        fprintf(stderr, "usage: lpcnet_demo -encode <input.pcm> <compressed.lpcnet>\n");
        fprintf(stderr, "       lpcnet_demo -decode <compressed.lpcnet> <output.pcm>\n");
        fprintf(stderr, "       lpcnet_demo -features <input.pcm> <features.f32>\n");
        fprintf(stderr, "       lpcnet_demo -synthesis <features.f32> <output.pcm>\n");
        fprintf(stderr, "       lpcnet_demo -plc <percent> <input.pcm> <output.pcm>\n");
        return 0;
    }
    if (strcmp(argv[1], "-encode") == 0) mode=MODE_ENCODE;
    else if (strcmp(argv[1], "-decode") == 0) mode=MODE_DECODE;
    else if (strcmp(argv[1], "-features") == 0) mode=MODE_FEATURES;
    else if (strcmp(argv[1], "-synthesis") == 0) mode=MODE_SYNTHESIS;
    else if (strcmp(argv[1], "-plc") == 0) {
        mode=MODE_PLC;
        plc_percent = atoi(argv[2]);
        argv++;
    } else if (strcmp(argv[1], "-plc_file") == 0) {
        mode=MODE_PLC;
        plc_file = fopen(argv[2], "r");
        argv++;
    } else {
        exit(1);
    }
    fin = fopen(argv[2], "rb");
    if (fin == NULL) {
	fprintf(stderr, "Can't open %s\n", argv[2]);
	exit(1);
    }

    fout = fopen(argv[3], "wb");
    if (fout == NULL) {
	fprintf(stderr, "Can't open %s\n", argv[3]);
	exit(1);
    }

    if (mode == MODE_ENCODE) {
        LPCNetEncState *net;
        net = lpcnet_encoder_create();
        while (1) {
            unsigned char buf[LPCNET_COMPRESSED_SIZE];
            short pcm[LPCNET_PACKET_SAMPLES];
            size_t ret;
            ret = fread(pcm, sizeof(pcm[0]), LPCNET_PACKET_SAMPLES, fin);
            if (feof(fin) || ret != LPCNET_PACKET_SAMPLES) break;
            lpcnet_encode(net, pcm, buf);
            fwrite(buf, 1, LPCNET_COMPRESSED_SIZE, fout);
        }
        lpcnet_encoder_destroy(net);
    } else if (mode == MODE_DECODE) {
        LPCNetDecState *net;
        net = lpcnet_decoder_create();
        while (1) {
            unsigned char buf[LPCNET_COMPRESSED_SIZE];
            short pcm[LPCNET_PACKET_SAMPLES];
            size_t ret;
            ret = fread(buf, sizeof(buf[0]), LPCNET_COMPRESSED_SIZE, fin);
            if (feof(fin) || ret != LPCNET_COMPRESSED_SIZE) break;
            lpcnet_decode(net, buf, pcm);
            fwrite(pcm, sizeof(pcm[0]), LPCNET_PACKET_SAMPLES, fout);
        }
        lpcnet_decoder_destroy(net);
    } else if (mode == MODE_FEATURES) {
        LPCNetEncState *net;
        net = lpcnet_encoder_create();
        while (1) {
            float features[NB_TOTAL_FEATURES];
            short pcm[LPCNET_FRAME_SIZE];
            size_t ret;
            ret = fread(pcm, sizeof(pcm[0]), LPCNET_FRAME_SIZE, fin);
            if (feof(fin) || ret != LPCNET_FRAME_SIZE) break;
            lpcnet_compute_single_frame_features(net, pcm, features);
            fwrite(features, sizeof(float), NB_TOTAL_FEATURES, fout);
        }
        lpcnet_encoder_destroy(net);
    } else if (mode == MODE_SYNTHESIS) {
        LPCNetState *net;
        net = lpcnet_create();
        while (1) {
            float in_features[NB_TOTAL_FEATURES];
            float features[NB_FEATURES];
            short pcm[LPCNET_FRAME_SIZE];
            size_t ret;
            ret = fread(in_features, sizeof(features[0]), NB_TOTAL_FEATURES, fin);
            if (feof(fin) || ret != NB_TOTAL_FEATURES) break;
            RNN_COPY(features, in_features, NB_FEATURES);
            lpcnet_synthesize(net, features, pcm, LPCNET_FRAME_SIZE);
            fwrite(pcm, sizeof(pcm[0]), LPCNET_FRAME_SIZE, fout);
        }
        lpcnet_destroy(net);
    } else if (mode == MODE_PLC) {
        short pcm[FRAME_SIZE];
        int count=0;
        int loss=0;
        LPCNetPLCState *net;
        net = lpcnet_plc_create();
        while (1) {
            size_t ret;
            ret = fread(pcm, sizeof(pcm[0]), FRAME_SIZE, fin);
            if (feof(fin) || ret != FRAME_SIZE) break;
            if (count % 2 == 0) {
              if (plc_file != NULL) fscanf(plc_file, "%d", &loss);
              else loss = rand() < RAND_MAX*(float)plc_percent/100.f;
            }
            if (loss) lpcnet_plc_conceal(net, pcm);
            else lpcnet_plc_update(net, pcm);
            //if (count)
              fwrite(pcm, sizeof(pcm[0]), FRAME_SIZE, fout);
            count++;
        }
        //lpcnet_plc_conceal(net, pcm);
        //fwrite(pcm, sizeof(pcm[0]), FRAME_SIZE, fout);
        lpcnet_plc_destroy(net);
    } else {
        fprintf(stderr, "unknown action\n");
    }
    fclose(fin);
    fclose(fout);
    return 0;
}
