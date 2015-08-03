#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgproc/imgproc_c.h"
//#include <dirent.h>
#include <iostream>
#include <regex>

#include "MeanShift.h"

using namespace cv;


int main1(int argc, char* argv[])
{
	//std::regex jpg_regex("[0-9]+\\.jpg");
	//string dirName = "C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\model\\pics";
	//DIR *dir;
	string a[5] = {"1", "2"};
	IplImage *imgarray[10];
	for (int i = 0; i < 10; i++) imgarray[i] = new IplImage();
	//imgarray[0] = cvLoadImage("C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\model\\pics\\44.jpg");
	//imgarray[0] = cvLoadImage("C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\from_feiche\\model\\pics\\39.jpg");
	imgarray[1] = cvLoadImage("C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\user1\\pics\\001left-1.jpg");
	imgarray[2] = cvLoadImage("C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\user1\\pics\\001middle.jpg");
	imgarray[3] = cvLoadImage("C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\user1\\pics\\001right-1.jpg");
	imgarray[4] = cvLoadImage("C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\user2\\pics\\002left-1.jpg");
	imgarray[5] = cvLoadImage("C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\user2\\pics\\002middle.jpg");
	imgarray[6] = cvLoadImage("C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\user2\\pics\\002right1.jpg");
	//imgarray[7] = cvLoadImage("C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\model\\pics\\8.jpg");
	for (int i = 1; i < 7; i++){
		IplImage *img = imgarray[i];
		// Mean shift
		int **ilabels = new int *[img->height];
		for (int i = 0; i<img->height; i++) ilabels[i] = new int[img->width];
		int regionCount = MeanShift(img, ilabels);
		vector<int> color(regionCount);
		CvRNG rng = cvRNG(cvGetTickCount());
		for (int i = 0; i<regionCount; i++)
			color[i] = cvRandInt(&rng);

		// Draw random color
		for (int i = 0; i<img->height; i++)
		for (int j = 0; j<img->width; j++)
		{
			int cl = ilabels[i][j];
			((uchar *)(img->imageData + i*img->widthStep))[j*img->nChannels + 0] = (color[cl]) & 255;
			((uchar *)(img->imageData + i*img->widthStep))[j*img->nChannels + 1] = (color[cl] >> 8) & 255;
			((uchar *)(img->imageData + i*img->widthStep))[j*img->nChannels + 2] = (color[cl] >> 16) & 255;
		}

		cvNamedWindow("MeanShift", CV_WINDOW_AUTOSIZE);
		cvShowImage("MeanShift", img);
		cvWaitKey();

		cvDestroyWindow("MeanShift");

		cvReleaseImage(&img);
	}





	return 0;
}
