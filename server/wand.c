#include <stdio.h>
#include <stdlib.h>
#include <wand/MagickWand.h>

#define ThrowWandException(wand) \
{ \
  char \
    *description; \
 \
  ExceptionType \
    severity; \
 \
  description=MagickGetException(wand,&severity); \
  (void) fprintf(stderr,"%s %s %lu %s\n",GetMagickModule(),description); \
  description=(char *) MagickRelinquishMemory(description); \
  exit(-1); \
}


int main(int argc,char **argv)
{

  MagickBooleanType status;

  MagickWand *magick_wand;

  if (argc != 3) {
	(void) fprintf(stdout,"Usage: %s image thumbnail\n",argv[0]);
      exit(0);
  }

  // init magick_wand.
  MagickWandGenesis();
  magick_wand = NewMagickWand();

  // Read an image.
  status = MagickReadImage(magick_wand,argv[1]);
  if (status == MagickFalse)
    ThrowWandException(magick_wand);

  // Turn the images into a thumbnail sequence.
  MagickResetIterator(magick_wand);

  while (MagickNextImage(magick_wand) != MagickFalse) {

	// Get the image's width and height
	int width = MagickGetImageWidth(magick_wand);
	int height = MagickGetImageHeight(magick_wand);
	fprintf(stderr, "get image w:%d h:%d\n", width, height);

    //MagickResizeImage(magick_wand,106,80,LanczosFilter,1.0);
	MagickResizeImage(magick_wand,100,100,LanczosFilter,1.0);
	fprintf(stderr, "resize image\n", width, height);

	// set the compression quality to 95 (high quality = low compression)
	MagickSetImageCompressionQuality(magick_wand, 95);

  	// Write the image then destroy it.
  	status = MagickWriteImages(magick_wand,argv[2],MagickTrue);
  	if (status == MagickFalse)
    	ThrowWandException(magick_wand);
  }

  // Clean up
  if (magick_wand)
  	magick_wand = DestroyMagickWand(magick_wand);

  MagickWandTerminus();

  return(0);
}
