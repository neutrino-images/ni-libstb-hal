#ifndef __PLAYBACK_LIB_H__
#define __PLAYBACK_LIB_H__

#include <string>
#include <stdint.h>
#include <vector>

typedef enum
{
	PLAYMODE_TS = 0,
	PLAYMODE_FILE,
} playmode_t;

struct AVFormatContext;
class cPlayback
{
	private:
		bool playing;
		int mAudioStream;
		int mSubtitleStream;
		int mTeletextStream;

	public:
		cPlayback(int);
		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char *filename, int vpid, int vtype, int apid, int ac3, int duration, std::string headers = "");
		bool Start(std::string filename, std::string headers = "");
		bool SetAPid(int pid, bool ac3);
		bool SetSubtitlePid(int pid);
		bool SetTeletextPid(int pid);
		int GetAPid(void) { return mAudioStream; }
		int GetVPid(void);
		int GetSubtitlePid(void) { return mSubtitleStream; }
		int GetTeletextPid(void);
		void SuspendSubtitle(bool);
		int GetFirstTeletextPid(void);
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);
		void GetPts(uint64_t &pts);
		bool SetPosition(int position, bool absolute = false);
		void FindAllPids(int *apids, unsigned int *ac3flags, unsigned int *numpida, std::string *language);
		void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language) { FindAllPids((int *) apids, (unsigned int *) ac3flags, (unsigned int *) numpida, language); };
		void FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language);
		void FindAllTeletextsubtitlePids(int *pids, unsigned int *numpidt, std::string *tlanguage, int *mags, int *pages);
		void RequestAbort(void);
		bool IsPlaying(void) { return false; }
		uint64_t GetReadCount(void);
		void FindAllSubs(int *pids, unsigned int *supported, unsigned int *numpida, std::string *language);
		void FindAllSubs(uint16_t *pids, unsigned short *supported, uint16_t *numpida, std::string *language) { FindAllSubs((int *) pids, (unsigned int *) supported, (unsigned int *) numpida, language); };
		bool SelectSubtitles(int pid, std::string charset = "");
		void GetTitles(std::vector<int> &playlists, std::vector<std::string> &titles, int &current);
		void SetTitle(int title);
		void GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
		void GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values);
		//
		~cPlayback();
		AVFormatContext *GetAVFormatContext() { return NULL; }
		void ReleaseAVFormatContext() {}
};

#endif // __PLAYBACK_LIB_H__
