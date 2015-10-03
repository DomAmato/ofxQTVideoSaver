#include "ofxQtVideoSaver.h"

ofxQtVideoSaver::ofxQtVideoSaver (){
	sResRefNum = 0;
	movie = nil;
	track = NULL;
	media = NULL;
	pMovieGWorld = NULL;
	pixMapHandle = NULL;
	pCompressedData = NULL;
	pSavedPort = NULL;
	hSavedDevice = NULL;
	hCompressedData = NULL;
	lMaxCompressionSize = 0L;
	hImageDescription = NULL;
	codecType			= kJPEGCodecType;   
	codecQualityLevel	= OF_QT_SAVER_CODEC_QUALITY_HIGH;
	bSetupForRecordingMovie = false;
}

		
void ofxQtVideoSaver::setCodecQualityLevel(int level) { 
	if (level <=  OF_QT_SAVER_CODEC_QUALITY_LOSSLESS && level >= 0){
		codecQualityLevel = level;
	} else {
		ofLogError("ofQTVideoSaver")  <<"please see the defines in ofQtSaver.h ";
	}
}


void ofxQtVideoSaver::setup( int width , int height, string movieName){

	w = width;
	h = height;
    
   
    fileName = (ofToDataPath(movieName));
    //pszFlatFilename = flatFileName;
    
    initializeQuicktime();
    	/*  Load the FSSpec structure to describe the receiving file.  For a 
    description of this and related calls see 
    http://developer.apple.com/quicktime/icefloe/dispatch004.html.
    ================================================================  */


	#ifdef TARGET_WIN32
		FILE * pFile;
		pFile = fopen (fileName.c_str(),"w");
		fclose (pFile);
		char fileNameStr[255];
		sprintf(fileNameStr, "%s", fileName.c_str());
		osErr = NativePathNameToFSSpec (fileNameStr, &fsSpec, 0);
		
	#endif
	#ifdef TARGET_OSX
	
		/// kill a file and make a new one if needed:		
		FILE * pFile;
		pFile = fopen (fileName.c_str(),"w");
		fclose (pFile);
	
		Boolean isdir;
		osErr = FSPathMakeRef((const UInt8*)fileName.c_str(), &fsref, &isdir);
		osErr = FSGetCatalogInfo(&fsref, kFSCatInfoNone, NULL, NULL, &fsSpec, NULL);
	#endif

    if (osErr && (osErr != fnfErr))    /* File-not-found error is ok         */
      { 
      ofLogError("ofQTVideoSaver")  <<"getting FSS spec failed " << osErr; 
      goto bail; 
     }
	 

	/*  Step 1:  Create a new, empty movie file and a movie that references that 
    file (CreateMovieFile).  
    ======================================================================== */
            
    osErr = CreateMovieFile 
      (
      &fsSpec,                         /* FSSpec specifier                   */
      FOUR_CHAR_CODE('TVOD'),          /* file creator type, TVOD = QT player*/
      smCurrentScript,                 /* movie script system to use         */
      createMovieFileDeleteCurFile     /* movie file creation flags          */
        | createMovieFileDontCreateResFile,
      &sResRefNum,                     /* returned file ref num to data fork */
      &movie                           /* returned handle to open empty movie*/
                                       /*   that references the created file */
      );
    if (osErr) 
      { 
      ofLogError("ofQTVideoSaver")  <<"CreateMovieFile failed " << osErr; 
      goto bail; 
      }


	/*  Step 2:  Add a new track to that movie (NewMovieTrack).
    =======================================================  */

    track = NewMovieTrack 
      (
      movie,                           /* the movie to add track to          */
      ((long) w << 16),              /* width of track in pixels (Fixed)   */
      FixRatio (h, 1),               /* height of track in pixels (Fixed)  */ 
	  kFullVolume// kNoVolume                        /* default volume level               */
      );
    osErr = GetMoviesError ();
    if (osErr) 
      { 
      ofLogError("ofQTVideoSaver")  <<"NewMovieTrack failed " << osErr; 
      goto bail; 
      }
    

	/*  Step 3:  Add a new media to that track (NewTrackMedia).
    =======================================================  */
    
    media = NewTrackMedia 
      (
      track,                           /* the track to add the media to      */
      VideoMediaType,                  /* media type, e.g. SoundMediaType    */
      600,                             /* num media time units that elapse/sec*/
      NULL,                            /* ptr to file that holds media sampls*/
      0                                /* type of ptr to media samples       */
      );
    osErr = GetMoviesError ();
    if (osErr) 
      { 
      ofLogError("ofQTVideoSaver")  <<"NewTrackMedia failed " << osErr; 
      goto bail; 
      }

	/*  Step 4:  Add media samples to the media. 
    ========================================  */
    
    BeginMediaEdits (media);           /* Inform the Movie Toolbox that we   */
                                       /*   want to change the media samples */
                                       /*   referenced by a track's media.   */
                                       /*   This opens the media container   */
                                       /*   and makes it ready to receive    */
                                       /*   and/or remove sample data.       */
    
    
    

    // Step 5: setup graphics port for qt movie and compression type ---
    
    /*  Create a new offscreen graphics world that will hold the movie's
    drawing surface.  draw_image() copies the image of IceFlow to this
    surface with varying amounts of transparency.
    =================================================================  */
    
    MacSetRect (&rect, 0, 0, w, h);

    osErr = NewGWorld 
      (
      &pMovieGWorld,                   /* receives the new GWorld.           */
      24,                              /* pixel depth in bits/pixel          */
      &rect,                           /* desired size of the GWorld.        */
      NULL, 
      NULL, 
      (GWorldFlags) 0
      );
    if (osErr != noErr) 
      { 
      ofLogError("ofQTVideoSaver")  <<"NewGWorld 1 failed " << osErr; 
      goto bail; 
      }


/*  Retrieve the pixel map associated with that graphics world and lock 
    the pixel map in memory.  GetMaxCompressionSize() and CompressImage()
    only operate on pixel maps, not graphics worlds.
    =====================================================================  */
    
    pixMapHandle = GetGWorldPixMap (pMovieGWorld);
    if (pixMapHandle == NULL) 
      { 
      ofLogError("ofQTVideoSaver")  <<"GetGWorldPixMap failed"; 
      goto bail; 
      }
    LockPixels (pixMapHandle);


/*  Get the maximum number of bytes required to hold an image having the 
    specified characteristics compressed using the specified compressor.
    ====================================================================  */

     
    osErr = GetMaxCompressionSize 
      (
      pixMapHandle,							/* the pixel map to compress from.    */
      &rect,								/* the image rectangle.               */
      0,									/* let ICM choose image bit depth.    */
      codecHighQuality,						/* compression quality specifier.     */
      kRawCodecType,						/* desired compression type           */   // < set to RAW in case we set to a new compression type...
      (CompressorComponent) anyCodec,		/* codec specifier.                   */
      &lMaxCompressionSize					/* receives max bytes needed for cmp. */
      );
    if (osErr != noErr) 
      { 
      ofLogError("ofQTVideoSaver")  <<"GetMaxCompressionSize failed" << osErr; 
      goto bail; 
      }


/*  Allocate a buffer to hold the compressed image data by creating a new
    handle.
    =====================================================================  */
    hCompressedData = NewHandle (lMaxCompressionSize);
    if (hCompressedData == NULL) 
      { 
      ofLogError("ofQTVideoSaver")  <<"NewHandle(%ld) failed" << lMaxCompressionSize; 
      goto bail; 
      }

/*  Lock the handle and then dereference it to obtain a pointer to the data 
    buffer because CompressImage() wants us to pass it a pointer, not a 
    handle. 
    =======================================================================  */

    HLockHi (hCompressedData);
    pCompressedData = *hCompressedData;

/*  Create an image description object in memory of minimum size to pass 
    to CompressImage().  CompressImage() will resize the memory as 
    necessary so create it small here.
    ====================================================================  */
    
    hImageDescription = (ImageDescriptionHandle) NewHandle (4);
    if (hImageDescription == NULL) 
      { 
      ofLogError("ofQTVideoSaver")  << "NewHandle(4) failed"; 
      goto bail; 
      }
	
	
	
	bSetupForRecordingMovie = true;
    return;
    
    
    
    
  bail:    
	ofLogError("ofQTVideoSaver")  << "got to bail somehows";
    if (sResRefNum != 0) CloseMovieFile (sResRefNum);
    if (movie     != NULL) DisposeMovie (movie);

    //ExitMovies ();                     /* Finalize Quicktime                 */
    
    return;
}

void ofxQtVideoSaver::addAudioTrack(string audioPath)
{	
	OSErr err;
	Handle dataRef = NULL;
	FSSpec    fileSpec;
	short audioMovieRefNum = 0;
	short audioMovieResId = 0;
	Movie audioMovie = NULL;
	Track audioCopyTrack = NULL;
	Media audioCopyMedia = NULL;
	Track destTrack = NULL;
	Media destMedia = NULL;
	
	destTrack = NewMovieTrack (movie, 0, 0, kFullVolume);
	destMedia = NewTrackMedia (destTrack, SoundMediaType,
							   30. * 100, /* Video Time Scale */
							   nil, 0);
	
	err = BeginMediaEdits (destMedia);
	if (err != noErr)
		ofLogError("QTAudio") << "Error beginning media edit";
	
	//make a buffer and fill it with the path name
	char * p = new char[audioPath.length()+1];
    strcpy(p, audioPath.c_str());
	//convert path to quicktime path
    NativePathNameToFSSpec(p, &fileSpec, 0L);
	
	//open file
	err = OpenMovieFile(&fileSpec, &audioMovieRefNum, fsRdPerm);
	if (err != noErr)
		ofLogError("QTAudio") << "Error opening movie file";
	
	//make a movie with the track information
	err = NewMovieFromFile(&audioMovie, audioMovieRefNum, &audioMovieResId, NULL, newMovieActive, NULL);
	if (err != noErr)
		ofLogError("QTAudio") << "Error making new movie";
	err = CloseMovieFile(audioMovieRefNum);
	if (err != noErr)
		ofLogError("QTAudio") << "Error closing movie";
	
	SetMovieTimeScale(audioMovie, 30.*100);
	
	audioCopyTrack = GetMovieTrack(audioMovie, 1);
	audioCopyMedia = GetTrackMedia(audioCopyTrack);
	
	long duration = GetMovieDuration(audioMovie);
	
	err = AddEmptyTrackToMovie (audioCopyTrack, movie, nil, nil, &destTrack);
	if (err != noErr)
		ofLogError("QTAudio") << "Error adding empty track";
	err = InsertTrackSegment(audioCopyTrack, destTrack, 0, duration, 0);
	if (err != noErr)
		ofLogError("QTAudio") << "Error inserting track";
	err = EndMediaEdits(destMedia);
	if (err != noErr)
		ofLogError("QTAudio") << "Error ending edits";
}



//--------------------------------------------------------------
void ofxQtVideoSaver::listCodecs(){

	initializeQuicktime();

	OSStatus error = noErr;
	CodecNameSpecListPtr list;
	CodecNameSpec * codecNameSpecPtr;
	char typeName[32];

	error = GetCodecNameList( &list, 0 );
	if ( error ) return;

	int numCodecs = list->count;
	codecNameSpecPtr = (CodecNameSpec *)((short *)list + 1);

	for (int i = 0; i < numCodecs; i++ ){
		p2cstrcpy( typeName, codecNameSpecPtr->typeName );
		ofLogError("ofQTVideoSaver")  << "codec" <<  i << ", " << typeName;
		codecNameSpecPtr++;
	}
	DisposeCodecNameList( list );
}

//--------------------------------------------------------------
void ofxQtVideoSaver::setCodecType( int chosenCodec ){

	initializeQuicktime();

	OSStatus error = noErr;
	CodecNameSpecListPtr list;
	CodecNameSpec * codecNameSpecPtr;
	char typeName[32];

	error = GetCodecNameList( &list, 0 );
	if ( error ) return;

	int numCodecs = list->count;
	codecNameSpecPtr = (CodecNameSpec *)((short *)list + 1);

	for (int i = 0; i < numCodecs; i++ ){
		if (i == chosenCodec){
			p2cstrcpy( typeName, codecNameSpecPtr->typeName );
			ofLogError("ofQTVideoSaver")  <<"trying to set codec type to " << typeName;
			codecType = codecNameSpecPtr->cType;
		}
		codecNameSpecPtr++;
	}
}


//-----------------------------------------------------------------------------
void ofxQtVideoSaver::finishMovie(){

	if (!bSetupForRecordingMovie) return;
	
	bSetupForRecordingMovie = false;
	
    
	osErr = EndMediaEdits(media);             /* Inform the Movie Toolbox that they */
                                       /*   can close the media container.   */
	if (osErr)
	{
		ofLogError("ofQTVideoSaver") << "Ending Media Resource failed " << osErr;
		goto bail;
	}

	/*  Step 5:  Insert a reference into the track that specifies which of the
    media samples to play and when to start playing them. 
    ======================================================================  */
    
	osErr = InsertMediaIntoTrack
      (
      track,                           /* the track to update.               */
      0,                               /* time in track where the specified  */
                                       /*   media samples should start playg */
                                       /*   using movie time scale.          */
      0,                               /* time in media samples of the first */
                                       /*   sample to play using media time  */
                                       /*   scale.                           */
      GetMediaDuration (media),        /* duration of media samples to play  */
                                       /*   using media time scale.          */
      1L<<16 //fixed1                  /* rate at which to play the samples. */
      );
	if (osErr)
	{
		ofLogError("ofQTVideoSaver")  <<"Inserting Media Resource failed " << osErr;
		goto bail;
	}

/*  Step 6:  Append the movie atom to the movie file (AddMovieResource).
    ====================================================================  */
    
    sResId = movieInDataForkResID;
    osErr = AddMovieResource
      (
      movie,                           /* movie to create moov atom from     */
      sResRefNum,                      /* file to receive the moov atom      */
      &sResId,                         /* id num of movie resource (res fork)*/
      (unsigned char *) fileName.c_str()                      /* name of movie resource (res fork)  */
      );
    if (osErr) 
      { 
      ofLogError("ofQTVideoSaver")  <<"AddMovieResource failed " << osErr; 
      goto bail; 
      }

    if (sResRefNum != 0) 
      {
      CloseMovieFile (sResRefNum);     /* close file CreateMovieFile opened  */
      sResRefNum = 0;
      }


	/*  Step 7 (optional):  Place the movie atom as the first atom in a new 
    movie file, and interleave the media data (FlattenMovieData).        
    ===================================================================  */
    
    // no flattening necessary I think .....  
	// if (bFlatten) flatten_my_movie (movie, pszFlatFilename);


	/*  Step 8:  Close the movie file that CreateMovieFile opened (if necessary) 
    and dispose of the movie memory structures (DisposeMovie). 
    ========================================================================  */
    


	SetGWorld (pSavedPort, hSavedDevice);
    DisposeMovie (movie);
	if (hImageDescription != NULL) DisposeHandle ((Handle) hImageDescription);
    if (hCompressedData   != NULL) DisposeHandle (hCompressedData);
    if (pMovieGWorld      != NULL) DisposeGWorld (pMovieGWorld);

    closeQuicktime();

  bail:    

    if (sResRefNum != 0) CloseMovieFile (sResRefNum);
    if (movie     != NULL) DisposeMovie (movie);

}

void ofxQtVideoSaver::addFrame(unsigned char* data, float frameLengthInSecs){
			
	if (!bSetupForRecordingMovie) return;

/*  Save the current GWorld and set the offscreen GWorld as current.
    ================================================================  */
    
    GetGWorld (&pSavedPort, &hSavedDevice);
    SetGWorld (pMovieGWorld, NULL);

	Ptr    gwAddress, gwAddressBase;
    long   gwWidth;
	float timeForQt = 0;
    gwAddressBase = GetPixBaseAddr( GetGWorldPixMap( pMovieGWorld ) );   /* Get head address of offscreen      */
    gwWidth = ( **GetGWorldPixMap( pMovieGWorld ) ).rowBytes & 0x3fff;   /* Get with of offscreen              */
    ///gwAddress = gwAddressBase + ( x * 3 ) + ( y * gwWidth );  /* Get adress for current pixel       */
    int myWidth = w*3;
    unsigned char * myData = data;

	#ifdef TARGET_OSX	
	//---------------------------------------------------------------
	// mac's have 32 bit no matter what, so we do it like this:
    for (int i = 0; i < h; i++){
		gwAddress = gwAddressBase + i * gwWidth;
		myData = data + i * myWidth;
		for (int j = 0; j < w; j++){
         memcpy(gwAddress+1, myData, 3);
         /*gwAddress[1] = myData[2];
         gwAddress[2] = myData[1];
         gwAddress[3] = myData[0];*/
         gwAddress+= 4;
         myData+= 3;
      } 
	}
	#endif 

	#ifdef TARGET_WIN32
	for (int i = 0; i < h; i++){
		gwAddress = gwAddressBase + i * gwWidth;
		myData = data + i * myWidth;
		memcpy(gwAddress, myData, myWidth);
	}
	#endif




	  /*    Compress the pixel map that has just been drawn on.  Also resize 
      and fill in the image description.  Resulting image size can be
      discovered by consulting the image description field dataSize.
      ================================================================  */
      
      osErr = CompressImage
        (
        pixMapHandle,                  /* the pixel map of the offscreen img */
        &rect,                         /* portion of the image to compress   */
        codecQualityLevel,             /* quality as set via default or #defines  */
        codecType,                     /* same codec specifier as above      */
        hImageDescription,             /* the created image description.     */
        pCompressedData                /* ptr to bufr that receives cmp image*/
        );
      if (osErr != noErr) 
        { 
        ofLogError("ofQTVideoSaver")  <<"CompressImage failed " << osErr; 
        goto bail; 
        }


/*    Add the compressed image to the movie.
      ======================================  */
      
	  // converting frame length to a time duration;
	  timeForQt = 1 / frameLengthInSecs;
	  
      osErr = AddMediaSample
        (
        media,                         /* the media to add the image to.     */
        hCompressedData,               /* the compressed image to add.       */
        0,			       /* byte offs into data to begin readg */
        (**hImageDescription).dataSize,/* num bytes to be copied into media. */
        600 / timeForQt,                      /* duration of the frame (media time) */
        (SampleDescriptionHandle) hImageDescription, /* image desc cast to   */
                                       /*   a sample description since both  */
                                       /*   both structures start with same  */
                                       /*   fields.                          */
        1,                             /* num samples in the data buffer.    */
        0,                             /* default flags                      */
        NULL                           /* ptr to receive media time in which */
                                       /*   the image was added.             */
        );
      if (osErr != noErr) 
        { 
        ofLogError("ofQTVideoSaver")  <<"AddMediaSample failed " << osErr; 
        //goto bail; 
        }
      

  return;
  
  bail:
  
    SetGWorld (pSavedPort, hSavedDevice);
    if (hImageDescription != NULL) DisposeHandle ((Handle) hImageDescription);
    if (hCompressedData   != NULL) DisposeHandle (hCompressedData);
    if (pMovieGWorld      != NULL) DisposeGWorld (pMovieGWorld);
}


//--------------------------------------------------------
void ofxQtVideoSaver::setGworldPixel( GWorldPtr gwPtr, int r, int g, int b, short x, short y){
	Ptr    gwAddress, gwAddressBase;
    long   gwWidth;
    char   red, blue, green;
    gwAddressBase = GetPixBaseAddr( GetGWorldPixMap( gwPtr ) );   /* Get head address of offscreen      */
    gwWidth = ( **GetGWorldPixMap( gwPtr ) ).rowBytes & 0x3fff;   /* Get with of offscreen              */
    gwAddress = gwAddressBase + ( x * 3 ) + ( y * gwWidth );  /* Get adress for current pixel       */
    *gwAddress = (unsigned char)r;                        /* Put red and move address forward   */
    *(gwAddress+1) = (unsigned char)g;                /* Put green and move address forward */
    *(gwAddress+2)   = (unsigned char)b;                       /* Put blue                           */
}
