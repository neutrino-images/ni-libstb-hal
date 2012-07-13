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
#if LIBAVFORMAT_VERSION_MAJOR > 53
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(avContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
#else
    AVMetadataTag *tag = NULL;
    while ((tag = av_metadata_get(avContext->metadata, "", tag, AV_METADATA_IGNORE_SUFFIX)))
#endif
        printf("%s: %s\n", tag->key, tag->value);
}


int main(int argc,char* argv[]) 
{
    char file[255] = {""};
    int  err, i;
    
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

#if LIBAVFORMAT_VERSION_MAJOR > 53
    if ((err = avformat_open_input(&avContext, file, NULL, 0)) != 0) {
#else
    if ((err = av_open_input_file(&avContext, file, NULL, 0, NULL)) != 0) {
#endif
        char error[512];

        printf("av_open_input_file failed %d (%s)\n", err, file);
        av_strerror(err, error, 512);
        printf("Cause: %s\n", error);

        return -1;
    }

    if (av_find_stream_info(avContext) < 0) 
    {
        printf("Error av_find_stream_info\n");
    }

    printf("\n***\n");
    dump_metadata();
     
    printf("\nstream specific metadata:\n");
    for (i = 0; i < avContext->nb_streams; i++)
    {
       AVStream* stream = avContext->streams[i];
    
       if (stream)
       {
#if LIBAVFORMAT_VERSION_MAJOR > 53
          AVDictionaryEntry *tag = NULL;
#else
          AVMetadataTag *tag = NULL;
#endif
          
          if (stream->metadata != NULL)
#if LIBAVFORMAT_VERSION_MAJOR > 53
             while ((tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
#else
             while ((tag = av_metadata_get(stream->metadata, "", tag, AV_METADATA_IGNORE_SUFFIX)))
#endif
                printf("%s: %s\n", tag->key, tag->value);
       }
    }
     
    return 0;
}
