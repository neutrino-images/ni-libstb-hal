/* konfetti
 * gpl
 * 2010
 *
 * example utitility to show metatags with ffmpeg.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

static AVFormatContext*   avContext = NULL;

void dump_metadata()
{
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(avContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
        printf("%s: %s\n", tag->key, tag->value);
}


int main(int argc,char* argv[])
{
    char file[255] = {""};
    unsigned int i;
    int  err;

    if (argc < 2)
    {
        printf("give me a filename please\n");
        return -1;
    }

    if (strstr(argv[1], "://") == NULL)
    {
        strcpy(file, "file://");
    }

    strcat(file, argv[1]);

    av_register_all();

    if ((err = avformat_open_input(&avContext, file, NULL, 0)) != 0) {
        char error[512];

        printf("avformat_open_input failed %d (%s)\n", err, file);
        av_strerror(err, error, 512);
        printf("Cause: %s\n", error);

        return -1;
    }

    if (avformat_find_stream_info(avContext, NULL) < 0)
    {
        printf("Error avformat_find_stream_info\n");
    }

    printf("\n***\n");
    dump_metadata();

    printf("\nstream specific metadata:\n");
    for (i = 0; i < avContext->nb_streams; i++)
    {
       AVStream* stream = avContext->streams[i];

       if (stream)
       {
          AVDictionaryEntry *tag = NULL;
          if (stream->metadata != NULL)
             while ((tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
                printf("%s: %s\n", tag->key, tag->value);
       }
    }

    return 0;
}
