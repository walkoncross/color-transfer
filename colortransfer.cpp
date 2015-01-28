#include <opencv2/opencv.hpp>
#include <iostream>

using namespace std;
using namespace cv;

#define fatalError(args...) \
  fprintf(stderr, args); \
  exit(1); \

/* global data */

// images
struct {
  Mat source, dest, transfer;
} images;

// windows
const string srcWin   = "Source Image";
const string dstWin   = "Original Target";
const string targWin  = "Modified Target";
const string controls = "Transfer Ratio";
const string readme   = "Instructions";

// trackbars
struct {
  string componentNames[3];
  int    componentVals[3];
} trackbars;

// mode
enum {
  NONE,
  LAB,
  RGB,
  HSV,
  XYZ
} currentMode = NONE;

/* color transfer function */
void colorTransfer() {
  int forwardConversion;
  int backwardConversion;

  switch (currentMode) {
    case LAB:
      forwardConversion = CV_BGR2Lab;
      backwardConversion = CV_Lab2BGR;
      break;
    case RGB:
      forwardConversion = CV_BGR2RGB;
      backwardConversion = CV_RGB2BGR;
      break;
    case HSV:
      forwardConversion = CV_BGR2HSV;
      backwardConversion = CV_HSV2BGR;
      break;
    case XYZ:
      forwardConversion = CV_BGR2XYZ;
      backwardConversion = CV_XYZ2BGR;
      break;
    default:
      forwardConversion = CV_BGR2Lab;
      backwardConversion = CV_Lab2BGR;
      break;
  }

  Mat subspaceSrc, subspaceDst, subspaceTransfer;

  /* convert images to colorspace used for transfer */
  cvtColor(images.source, subspaceSrc, forwardConversion); 
  cvtColor(images.dest, subspaceDst, forwardConversion);

  /* convert to floating point */
  subspaceSrc.convertTo(subspaceSrc, CV_32FC3);
  subspaceDst.convertTo(subspaceDst, CV_32FC3);

  /* convert to log space */
  log(subspaceSrc, subspaceSrc);
  log(subspaceDst, subspaceDst);

  /* compute statistics */
  vector<double> srcAvgs, srcDevs, dstAvgs, dstDevs;
  meanStdDev(subspaceSrc, srcAvgs, srcDevs);
  meanStdDev(subspaceDst, dstAvgs, dstDevs);

  /* split into indiv. channels */
  vector<Mat> srcComponents, dstComponents;
  split(subspaceSrc, srcComponents);
  split(subspaceDst, dstComponents);

  /* do the transfer */
  for(int component = 0; component < 3; component++) {
    double rate = (double)trackbars.componentVals[component] / 100.0;

    /* statistical transfer based on 
     * http://www.cs.tau.ac.il/~turkel/imagepapers/ColorTransfer.pdf
     */
    Mat modifiedComponent = 
        (srcDevs[component] / dstDevs[component])
      * (dstComponents[component] - dstAvgs[component])
      + srcAvgs[component];

    /* blend original with modified based on current (per-channel) rate */
    dstComponents[component] = 
      dstComponents[component] * (1-rate) + modifiedComponent * rate;
  }

  /* merge modified components into multichannel image */
  merge(dstComponents, subspaceTransfer);

  /* raise back to linear space */
  exp(subspaceTransfer, subspaceTransfer);

  /* convert to integral space */
  subspaceTransfer.convertTo(subspaceTransfer, CV_8UC3);

  /* convert back to original colorspace */
  cvtColor(subspaceTransfer, images.transfer, backwardConversion);
}


/* trackbar callback / update function */
void updateTransfer() {

  colorTransfer();
  imshow(targWin, images.transfer);

}

/* keypress parser */
int changeMode(char keypress) {
  switch(keypress) {
    case 'r':
    case 'R':
      if (currentMode == RGB) return 1;
      currentMode = RGB;
      trackbars.componentNames[0] = "Red";
      trackbars.componentNames[1] = "Green";
      trackbars.componentNames[2] = "Blue";
      break;
    case 'l':
    case 'L':
      if (currentMode == LAB) return 1;
      currentMode = LAB;
      trackbars.componentNames[0] = "Luminance";
      trackbars.componentNames[1] = "Alpha";
      trackbars.componentNames[2] = "Beta";
      break;
    case 'h':
    case 'H':
      if (currentMode == HSV) return 1;
      currentMode = HSV;
      trackbars.componentNames[0] = "Hue";
      trackbars.componentNames[1] = "Saturation";
      trackbars.componentNames[2] = "Value";
      break;
    case 'x':
    case 'X':
      if (currentMode == XYZ) return 1;
      currentMode = XYZ;
      trackbars.componentNames[0] = "X";
      trackbars.componentNames[1] = "Y";
      trackbars.componentNames[2] = "Z";
      break;
    case 27: // escape
      return 0;
    default:
      return 1;
  }

  /* refresh trackbars on mode change */
  destroyWindow(controls);
  namedWindow(controls, WINDOW_NORMAL);
  moveWindow(controls, images.dest.cols + 10, images.dest.rows + 155);

  for(int i = 0; i < 3; ++i) {
    trackbars.componentVals[i] = 100;                    // Initial value
    createTrackbar( trackbars.componentNames[i]          // Trackbar name
                  , controls                             // Window
                  , &trackbars.componentVals[i]          // Value ptr
                  , 100                                  // Max value
                  , (void(*)(int, void*))updateTransfer  // Callback
                  );
  }

  resizeWindow(controls, 600, 125);

  // force transfer update
  updateTransfer();

  return 1;
}

int main(int argc, char ** argv) {
  if(argc < 3 || argc > 4) {
    fatalError(
      "\n"
      "Usage: %s reference-input-file target-input-file [output-file]\n"
      "\n"
      "Starts the color transfer GUI with the supplied reference and target.\n"
      "If output-file is supplied, it is overwritten with the current result\n"
      "of the transfer at the time the user quits the program.\n"
      "\n",
      argv[0]
    ); 
  }

  /* read in image data */
  images.source = imread(argv[1], -1);
  images.dest   = imread(argv[2], -1);

  if ( images.source.empty() || images.dest.empty() ) {
    fatalError("Image data missing");
  } else if(images.source.channels() < 3 || images.dest.channels() < 3) {
    fatalError("One of source/dest may not be a color image");
  }
  
  /* create windows */
  namedWindow(srcWin,   WINDOW_AUTOSIZE);
  namedWindow(dstWin,   WINDOW_AUTOSIZE);
  namedWindow(targWin,  WINDOW_AUTOSIZE);
  namedWindow(controls, WINDOW_NORMAL); 
  namedWindow(readme,   WINDOW_AUTOSIZE);

  /* move windows */
  moveWindow(srcWin, 0, 0);
  moveWindow(dstWin, images.source.cols + 10, 0);
  moveWindow(targWin, 0, images.dest.rows + 50);
  moveWindow(readme, images.dest.cols + 10, images.dest.rows + 200);

  /* create "readme" window */

  #define NUMLINES 4
  const char * textlines[NUMLINES] =  {
    "Keymap:"
  , "'L' -> LAB, 'R' -> RGB" 
  , "'H' -> HSV, 'X' -> XYZ"
  , "ESC -> Save and Exit"
  };

  Mat text(25 * (NUMLINES + 1), 225, CV_8UC1, Scalar(255));

  for(int i = 0, x = 25; i < NUMLINES; i += 1, x += 25) {
    putText(text, textlines[i], Point(10, x), FONT_HERSHEY_PLAIN, 0.75, 0);
  }


  /* show originals and readme */
  imshow(srcWin, images.source);
  imshow(dstWin, images.dest);
  imshow(readme, text);

  /* initial space: LAB */
  changeMode('l');

  /* change modes on key press */
  while(changeMode(waitKey(0)));

  /* clean up */
  destroyAllWindows();
  if(argc == 4) {
    imwrite(argv[3], images.transfer);
  }
  return 0;
}



