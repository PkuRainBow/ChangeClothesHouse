#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/photo/photo.hpp"
#include "inpainter.h"

#include <iostream>
#include <fstream>

using namespace cv;
using namespace std;


void inpaint(Mat img0, int headHeight, Mat inpaintMask, Mat mask1, int halfPatchWidth, Mat &inpaintResult)
{
	Range R1;
	R1.start = 0;
	R1.end = headHeight + 1;
	cout << "height: " << headHeight << endl;
	Mat img = Mat::Mat(img0, R1, Range::all());
	Mat maskROI = Mat::Mat(mask1, R1, Range::all());
	Mat inpainted;
	Inpainter i(img, maskROI, halfPatchWidth);
	if (i.checkValidInputs() != i.CHECK_VALID){
		cout << "error" << endl;
		return;
	}
	i.inpaint();
	inpainted = i.result;
	inpaintResult = img0.clone();
	for (int i = 0; i < inpainted.rows; i++)
	{
		for (int j = 0; j < inpainted.cols; j++)
		if (inpaintMask.at<uchar>(i, j) > 0)
		{
			for (int k = 0; k < 3; k++)
				inpaintResult.at<uchar>(i, j * 3 + k) = inpainted.at<uchar>(i, j * 3 + k);
		}
	}
	int Debug = 0;
}

int main(int argc, char** argv)
{

	string dir = "";
	int halfPatchWidth = 4;

	ifstream fp("E:\\FashionRoom\\test\\output\\model2\\pics\\height.txt");

	int flag[700];
	for (int i = 0; i < 700; i++)
		flag[i] = 0;

	for (int i = 0; i < 412; i++)
	{
		int x, y;
		fp >> x >> y;
		flag[x] = y;
	}

	for (int i = 1; i < 600; i++)
	{
		if (flag[i] > 0)
		{
			cout << i << endl;
			stringstream ss;
			ss << i;
			ss >> dir;
			string  filename = "E:\\FashionRoom\\test\\output\\model2\\pics\\" + dir + ".jpg";
			ifstream fin(filename);
			if (!fin){
				continue;
			}


			Mat img0 = imread(filename, -1);

			/*		Range R1;
			R1.start = 0;
			R1.end = flag[i];

			Mat img = Mat::Mat(img0, R1, Range::all());  */
			Mat inpaintMask;

			inpaintMask = imread("E:\\FashionRoom\\test\\output\\model2\\pics\\" + dir + "_3.bmp", 0);

			Mat mask1 = imread("E:\\FashionRoom\\test\\output\\model2\\pics\\" + dir + "_1.bmp", 0);

			//	Mat maskROI = Mat::Mat(mask1, R1, Range::all());

			/*	Mat inpainted;
			//inpaint(img, maskROI, inpainted, 3, CV_INPAINT_TELEA);
			Inpainter i(img, maskROI, halfPatchWidth);
			if (i.checkValidInputs() != i.CHECK_VALID){
			cout << "error" << endl;
			continue;
			}
			i.inpaint();
			inpainted = i.result;
			Mat result = img0.clone();
			for (int i = 0; i < img0.rows; i++)
			for (int j = 0; j < img0.cols; j++)
			if (inpaintMask.at<uchar>(i, j) > 0)
			{
			for (int k = 0; k < 3; k++)
			result.at<uchar>(i, j * 3 + k) = inpainted.at<uchar>(i, j * 3 + k);
			}    */
			Mat result;
			inpaint(img0, flag[i], inpaintMask, mask1, halfPatchWidth, result);
			imwrite("E:\\FashionRoom\\test\\output\\model2\\result\\" + dir + ".jpg", result);

		}
	}

	return 0;
}
