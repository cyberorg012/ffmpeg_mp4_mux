/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat API example.
 *
 * Output a media file in any supported libavformat format. The default
 * codecs are used.
 * @example muxing.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include<sys/time.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 1000*1000
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
    AVStream *st;
} OutputStream;

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    //av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int i;

	*codec = NULL;
	if(codec_id == AV_CODEC_ID_H264){
		AVCodec ff_fakeh264_encoder = {
			.name             = "libx264",
			.long_name        = "libx264 H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10",
			.type             = AVMEDIA_TYPE_VIDEO,
			.id               = AV_CODEC_ID_H264,
			.priv_data_size   = sizeof(1000),
			.init             = NULL,
			.encode2          = NULL,
			.close            = NULL,
			.capabilities     = CODEC_CAP_DELAY | CODEC_CAP_AUTO_THREADS,
			.priv_class       = NULL,
			.defaults         = NULL,
			.init_static_data = NULL,
		};

		*codec = &ff_fakeh264_encoder;
	}else{
		AVCodec ff_fake_amrnb_encoder = {
			.name           = "libopencore_amrnb",
			.long_name      = "OpenCORE AMR-NB (Adaptive Multi-Rate Narrow-Band)",
			.type           = AVMEDIA_TYPE_AUDIO,
			//.id             = AV_CODEC_ID_AMR_NB,
			.id             = AV_CODEC_ID_AAC,
			.priv_data_size = sizeof(1000),
			.init           = NULL,
			.encode2        = NULL,
			.close          = NULL,
			.capabilities   = CODEC_CAP_DELAY | CODEC_CAP_SMALL_LAST_FRAME,
			.sample_fmts    = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_S16,
															 AV_SAMPLE_FMT_NONE },
			.priv_class     = NULL,
		};
		*codec = &ff_fake_amrnb_encoder;
	}

	printf("avformat_new_stream\n");

    ost->st = avformat_new_stream(oc, *codec);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams-1;
    c = ost->st->codec;

    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = 64000;
        c->sample_rate = 44100;
        if ((*codec)->supported_samplerates) {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                if ((*codec)->supported_samplerates[i] == 44100)
                    c->sample_rate = 44100;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_STEREO;
        if ((*codec)->channel_layouts) {
            c->channel_layout = (*codec)->channel_layouts[0];
            for (i = 0; (*codec)->channel_layouts[i]; i++) {
                if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;

        c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width    = 352;
        c->height   = 288;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
        c->time_base       = ost->st->time_base;

        c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt       = STREAM_PIX_FMT;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
            /* just for testing, we also add B frames */
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }
    break;

    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
}

uint64_t begin_timestamp_us = 0;

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
    int ret;
    AVFrame *frame;
    int got_packet;
    AVCodecContext *c;
    c = ost->st->codec;
    int dst_nb_samples;

    if (1) {
		static audio_frame = 0;
		char filename[256];
		snprintf(filename, 256, "./aac_raw/aac_raw_%03d.aac", audio_frame);

		char buf[1024*50];
		AVPacket pkt = { 0 };
		av_init_packet(&pkt);

		FILE *f= fopen(filename, "rb");
		int len = 0;
		if(f){
			fseek(f, 0, SEEK_END); // seek to end of file
			len = ftell(f); // get current file pointer
			rewind(f);//fseek(f, 0, SEEK_SET); // seek back to beginning of file
			fread(buf, 1, len, f);
			fclose(f);
		}else{
			return 1;
		}
		
		struct timeval now;
		gettimeofday(&now, NULL);
		uint64_t timestamp_us = (now.tv_sec*1000000LL + now.tv_usec - begin_timestamp_us);
        //timestamp_us = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        //ost->samples_count += dst_nb_samples;
		printf("audio ts =%lld\n", timestamp_us);

		pkt.stream_index  = ost->st->index;
		pkt.pts = pkt.dts = timestamp_us;
		pkt.size = len;
		pkt.data = buf;
		got_packet = 1;

		audio_frame++;
		
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error while writing audio frame: %s\n",
                    av_err2str(ret));
            exit(1);
        }
    }

    return (frame || got_packet) ? 0 : 1;
}

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_video_frame(AVFormatContext *oc, OutputStream *ost)
{
    int ret;
    AVCodecContext *c;    
    c = ost->st->codec;

	struct timeval now;
	gettimeofday(&now, NULL);
	uint64_t timestamp_us = (now.tv_sec*1000000LL + now.tv_usec - begin_timestamp_us);

	{
		if(1){
			{
				AVFormatContext *fmt_ctx = oc; 
				const AVRational *time_base = &c->time_base; 
				AVStream *st = ost->st;
				
				char buf[1024*50];

				AVPacket pkt = { 0 };
				av_init_packet(&pkt);

				char filename[256];
				static int pts = 0;

				snprintf(filename, 256, "avc_raw/avc_raw_%03d.h264", pts);
				//snprintf(filename, 256, "/home/gjwang/video/s802/avc_raw_%03d.h264", pts);

				FILE *f= fopen(filename, "rb");
				int len = 0;
				if(f){
					fseek(f, 0, SEEK_END); // seek to end of file
					len = ftell(f); // get current file pointer
					//fseek(f, 0, SEEK_SET); // seek back to beginning of file
					rewind(f);
					fread(buf, 1, len, f);
					fclose(f);
				}else{
					return 1;
				}

				int nal_type = buf[4] & 0x0f;
				int is_IDR_nal = 0;
				if(nal_type == 0x7){
					//7 sps; 8 pps; IDR frame follow with sps
					is_IDR_nal = 1;
					pkt.flags        |= AV_PKT_FLAG_KEY;
					printf("frame %d is IDR\n", pts);
				}
				
				pkt.stream_index  = ost->st->index;
				pkt.data          = buf;
				pkt.size          = len;

				//pkt.pts = pkt.dts = pts*1000*1000;
				pkt.pts = pkt.dts = timestamp_us;
				pts++;

				write_frame(fmt_ctx, &c->time_base, st, &pkt);
			}
        }
    }

    if (ret < 0) {
        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        exit(1);
    }

    //return (frame || got_packet) ? 0 : 1;
	return 0;
}

/**************************************************************/
/* media file output */

int main(int argc, char **argv)
{
    OutputStream video_st = { 0 }, audio_st = { 0 };
    const char *filename;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodec *audio_codec, *video_codec;
    int ret;
    int encode_video = 0, encode_audio = 0;
    AVDictionary *opt = NULL;

    /* Initialize libavcodec, and register all codecs and formats. */
    av_register_all();

    if (argc < 2) {
        printf("usage: %s output_file\n"
               "API example program to output a media file with libavformat.\n"
               "This program generates a synthetic audio and video stream, encodes and\n"
               "muxes them into a file named output_file.\n"
               "The output format is automatically guessed according to the file extension.\n"
               "Raw images can also be output by using '%%d' in the filename.\n"
               "\n", argv[0]);
        return 1;
    }

    filename = argv[1];
    if (argc > 3 && !strcmp(argv[2], "-flags")) {
        av_dict_set(&opt, argv[2]+1, argv[3], 0);
    }

	extern AVOutputFormat ff_mp4_muxer;
	ff_mp4_muxer.video_codec = AV_CODEC_ID_H264;
	ff_mp4_muxer.audio_codec = AV_CODEC_ID_AAC,
	//ff_mp4_muxer.audio_codec = AV_CODEC_ID_AMR_NB;

    /* allocate the output media context */
    //avformat_alloc_output_context2(&oc, NULL, NULL, filename);
	avformat_alloc_output_context2(&oc, &ff_mp4_muxer, "mpeg", filename);
    if (!oc)
        return 1;

    fmt = oc->oformat;

    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
        encode_video = 1;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
        encode_audio = 1;
    }

    av_dump_format(oc, 0, filename, 1);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s': %s\n", filename,
                    av_err2str(ret));
            return 1;
        }
    }

    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",
                av_err2str(ret));
        return 1;
    }

	struct timeval now;
	gettimeofday(&now, NULL);
	begin_timestamp_us = (now.tv_sec*1000000LL + now.tv_usec);

    while (encode_video || encode_audio) {
        encode_video = !write_video_frame(oc, &video_st);
        encode_audio = !write_audio_frame(oc, &audio_st);
		usleep(1000);
    }

    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);

    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_close(oc->pb);

    /* free the stream */
    avformat_free_context(oc);

    return 0;
}
