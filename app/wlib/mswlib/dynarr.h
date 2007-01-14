typedef struct {
		int cnt;
		int max;
		void * ptr;
		} dynArr_t;

#define DYNARR_APPEND(T,DA,INCR) \
		{ if ((DA).cnt >= (DA).max) { \
			(DA).max += INCR; \
			(DA).ptr = realloc( (DA).ptr, (DA).max * sizeof *(T*)NULL ); \
			if ( (DA).ptr == NULL ) \
				abort(); \
		} \
		(DA).cnt++; }
#define DYNARR_ADD(T,DA,INCR) DYNARR_APPEND(T,DA,INCR)

#define DYNARR_LAST(T,DA) \
		(((T*)(DA).ptr)[(DA).cnt-1])
#define DYNARR_N(T,DA,N) \
		(((T*)(DA).ptr)[N])
#define DYNARR_RESET(T,DA) \
		(DA).cnt=0
#define DYNARR_SET(T,DA,N) \
		{ if ((DA).max < N) { \
			(DA).max = N; \
			(DA).ptr = realloc( (DA).ptr, (DA).max * sizeof *(T*)NULL ); \
			if ( (DA).ptr == NULL ) \
				abort(); \
		} \
		(DA).cnt = 0; }


#ifdef WINDOWS
#ifndef WIN32
#define FAR _far
#endif
#define M_PI 3.14159
#define strcasecmp _stricmp
#else
#endif
