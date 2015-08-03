#include <iostream>
#include <cstdio>
#include <io.h>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <tchar.h>
#include <Windows.h>
#include <fstream>

#include <cv.h>
#include <opencv.hpp>
#include <opencv2\opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "Microsoft.FaceSdk.Desktop.h"
#include "WICRenderer.h"
#include "HighResolutionTimer.h"
#include "WICHelper.h"
#include "FolderFileHelper.h"
#include "CommandLineArgument.h"

#include "tinyxml.h"
#include "tinystr.h"
#include "HairMarker.h"
#include "Inpainter.h"

using namespace std;
using namespace cv;


GCApplication hairMarker;

//inpaint module implementation
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
	if (i.checkValidInputs() != i.CHECK_VALID) {
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


static void on_mouse(int event, int x, int y, int flags, void* param)
{
	hairMarker.mouseClick(event, x, y, flags, param);
}

//??hairmarker????????bgdPxls??XML?????&mask?
bool saveLabel(GCApplication &hairmarker, Mat &mask, string outFilename, bool save)
{
	if (hairmarker.fgdPxls.empty() && hairmarker.bgdPxls.empty())
		return false;
	try
	{
		if (save) {	//?????
			TiXmlDocument *myDocument = new TiXmlDocument();
			TiXmlElement *rootElement = new TiXmlElement("HairPoints");
			myDocument->LinkEndChild(rootElement);
			for (vector<Point>::iterator it = hairmarker.fgdPxls.begin(); it != hairmarker.fgdPxls.end(); it++) {
				TiXmlElement *hairPoints = new TiXmlElement("Point");
				rootElement->LinkEndChild(hairPoints);
				double x = double(it->x) / mask.cols, y = double(it->y) / mask.rows;
				hairPoints->SetAttribute("x", x);
				hairPoints->SetAttribute("y", y);

				circle(mask, *it, 1, GC_FGD, -1);	//??mask?????????
			}
			myDocument->SaveFile();
		}
		else {	//??????
			for (vector<Point>::iterator it = hairmarker.fgdPxls.begin(); it != hairmarker.fgdPxls.end(); it++) {
				circle(mask, *it, 1, GC_FGD, -1);	//??mask?????????
			}
			for (vector<Point>::iterator it = hairmarker.bgdPxls.begin(); it != hairmarker.bgdPxls.end(); it++) {
				circle(mask, *it, 1, GC_BGD, -1);	//??mask?????????
			}
		}
	}
	catch (string &e) {
		return false;
	}
	return true;
}


// get a square up of the shoulder, using orientation of the face
void findSquare(vector<Point> &iCord, vector<Point> facePoints)
{
	int num = iCord.size();
	Point lPoint = iCord[0];
	Point rPoint = iCord[num - 1];

	int a = rPoint.x - lPoint.x;
	int b = lPoint.y - rPoint.y;
	int c, d;
	if (facePoints.size() != 0)
	{
		c = 10 * (facePoints[27].x - facePoints[57].x);
		d = 10 * (facePoints[57].y - facePoints[27].y);
	}
	else {
		c = -b;
		d = a;
	}

	Point newPoint = Point(rPoint.x + c, rPoint.y - d);
	//iCord.push_back(rPoint);
	iCord.push_back(newPoint);
	newPoint.x = newPoint.x - a;
	newPoint.y = newPoint.y + b;
	iCord.push_back(newPoint);
	//iCord.push_back(lPoint);
	//iCord.push_back(iCord[0]);
}

// get a square up of the shoulder, using orientation of the face
void findSquare_txt(vector<Point> &iCord, vector<Point> facePoints)
{
	int num = iCord.size();
	Point lPoint = iCord[0];
	Point rPoint = iCord[num - 1];

	int a = rPoint.x - lPoint.x;
	int b = lPoint.y - rPoint.y;
	int c, d;
	if (facePoints.size() != 0)
	{
		c = 10 * (facePoints[23].x - facePoints[24].x);	//23???68???27?,24???68???33?
		d = 10 * (facePoints[24].y - facePoints[23].y);
	}
	else {
		c = -b;
		d = a;
	}

	Point newPoint = Point(rPoint.x + c, rPoint.y - d);
	iCord.push_back(newPoint);
	newPoint.x = newPoint.x - a;
	newPoint.y = newPoint.y + b;
	iCord.push_back(newPoint);
}

// ?????????????
void findGoodChin(int &st, int &ed, vector<float> &lm, string imgSrc)
{
	st = 3;
	ed = 14;
	return;
}

// use FaceSdk to find the points of the chin and save in ret
void findChin(string imgSrc, IFaceSdkInterface* detector, IFaceSdkInterface* alignmentor, UINT lmCount, vector<Point> &ret, vector<Point> &facePoints)
{
	HRESULT hr = S_OK;
	WICHelper                   wic;        // WIC helper, providing image access functions.
	WICBitmapData               data;       // image data buffer.
	CComPtr<IWICBitmapSource>   bitmap;

	wchar_t* iName = new wchar_t[imgSrc.length() + 1];
	for (int i = 0; i < imgSrc.length(); i++)
		iName[i] = (wchar_t)imgSrc[i];
	iName[imgSrc.length()] = '\0';

	if (SUCCEEDED(hr))
	{
		// Load image from file and convert it to 8bpp grayscale image.
		hr = wic.CreateGrayscaleBitmap(iName, &bitmap);
	}

	if (SUCCEEDED(hr))
	{
		// data.pixels will point to image buffer after this function returns.
		// DO call wic.Unlock before applying any other image reading or modifying 
		// operations against the same IWICBitmapSource object.
		hr = wic.Lock(bitmap, &data);
	}

	HighResolutionTimer         timer;
	const UINT		            MaxFaceCount = 64;      // Assume that at maximum 64 faces will be processed in the given image.
	FaceRect			        faceRects[MaxFaceCount] = { 0 };
	UINT				        detectedCount = 0;      // Actual number of faces detected.
	UINT				        returnedCount = 0;      // The number of faces written to the input buffer.

	timer.Reset();

	if (SUCCEEDED(hr))
	{
		// Detect faces
		hr = Detect(detector, data.pixels, data.width, data.height, data.stride, faceRects, MaxFaceCount, &returnedCount, &detectedCount);
	}

	double duration = timer.Duration();
	ret.clear();
	if (detectedCount != 1)
	{
		wcout << "no face detected " << data.width << " " << iName << endl;
		return;
	}

	vector<float> landmarks(lmCount * 2);

	RECT rc = { faceRects[0].Left, faceRects[0].Top, faceRects[0].Right, faceRects[0].Bottom };
	float score;
	hr = Align(alignmentor, data.pixels, data.width, data.height, data.stride, rc, &landmarks[0], lmCount * 2, &score);

	wic.Unlock(&data);

	// add the chin points to the vector

	int st, ed;

	findGoodChin(st, ed, landmarks, imgSrc);
	for (int i = st; i < ed; i++)
	{
		Point iPoint = Point((int)landmarks[i * 2], (int)landmarks[i * 2 + 1]);
		ret.push_back(iPoint);
	}

	facePoints.clear();
	for (int i = 0; i < lmCount; i++)
	{
		Point iPoint = Point((int)landmarks[i * 2], (int)landmarks[i * 2 + 1]);
		facePoints.push_back(iPoint);
	}
}

// insert chin points to the shoulder vector
void insertChin(vector<Point> &iCord, string fileName, IFaceSdkInterface* detector, IFaceSdkInterface* alignmentor, UINT landmarkCount, vector<Point> &facePoints)
{
	HRESULT				    hr = S_OK;

	if (SUCCEEDED(hr))
	{
		vector<Point> chinPoints;
		findChin(fileName, detector, alignmentor, landmarkCount, chinPoints, facePoints);
		vector<Point> tmp;
		tmp.push_back(iCord[3]);
		tmp.push_back(iCord[4]);
		tmp.push_back(iCord[5]);
		iCord.pop_back();
		iCord.pop_back();
		iCord.pop_back();
		for (int i = 0; i < chinPoints.size(); i++)
		{
			iCord.push_back(chinPoints[i]);
		}
		iCord.push_back(tmp[0]);
		iCord.push_back(tmp[1]);
		iCord.push_back(tmp[2]);
	}

}

// get two masks of a picture
void getMask(vector<Point> &iCord, vector<Point> &sCord, vector<Point> &facePoints)
{
	int num = iCord.size();
	//lPoint rPoint???????????????
	Point lPoint = Point(iCord[0].x - (int)(0.5 * (iCord[1].x - iCord[0].x)), iCord[0].y);
	Point rPoint = Point(iCord[num - 1].x + (int)(0.5 * (iCord[num - 1].x - iCord[num - 2].x)), iCord[num - 1].y);
	iCord.insert(iCord.begin(), lPoint);
	iCord.push_back(rPoint);
	num = iCord.size();
	// add a point to sCord
	sCord.push_back(iCord[0]);
	if (iCord[0].y != iCord[num - 1].y)
	{
		Point newPoint;
		if (iCord[0].y > iCord[num - 1].y)
		{
			newPoint = Point(iCord[num - 1].x, iCord[0].y);
		}
		else {
			newPoint = Point(iCord[0].x, iCord[num - 1].y);
		}
		sCord.push_back(newPoint);
	}
	sCord.push_back(iCord[num - 1]);
	findSquare(iCord, facePoints);
	findSquare(sCord, facePoints);
}

// get two masks of a picture txt
void getMask_txt(vector<Point> &iCord, vector<Point> &sCord, vector<Point> &facePoints)
{
	int num = iCord.size();
	//lPoint rPoint???????????????
	Point lPoint = Point(iCord[0].x - (int)(1.0 * (iCord[1].x - iCord[0].x)), iCord[0].y);
	Point rPoint = Point(iCord[num - 1].x + (int)(1.0 * (iCord[num - 1].x - iCord[num - 2].x)), iCord[num - 1].y);
	iCord.insert(iCord.begin(), lPoint);
	iCord.push_back(rPoint);
	num = iCord.size();
	// add a point to sCord
	sCord.push_back(iCord[0]);
	if (iCord[0].y != iCord[num - 1].y)
	{
		Point newPoint;
		if (iCord[0].y > iCord[num - 1].y)
		{
			newPoint = Point(iCord[num - 1].x, iCord[0].y);
		}
		else {
			newPoint = Point(iCord[0].x, iCord[num - 1].y);
		}
		sCord.push_back(newPoint);
	}
	sCord.push_back(iCord[num - 1]);
	findSquare_txt(iCord, facePoints);
	findSquare_txt(sCord, facePoints);
}

//Grabcut??comMask?4??0/1/2/3??????binMask?2??0/1??0 2 -> 0; 1 3 -> 1;
void getBinMask(const Mat& comMask, Mat& binMask)
{
	if (comMask.empty() || comMask.type() != CV_8UC1)
		CV_Error(CV_StsBadArg, "comMask is empty or has incorrect type (not CV_8UC1)");
	if (binMask.empty() || binMask.rows != comMask.rows || binMask.cols != comMask.cols)
		binMask.create(comMask.size(), CV_8UC1);
	binMask = comMask & 1;
}

//????
void adjShoulder(vector<Point> &iCord, vector<Point> &facePoints) {
	//????????: iCord[2]??facePoints[3]?facePoints[4]????; iCord[num - 3]??facePoints[13]?facePoints[12]????
	//??iCord[2]?iCord[num - 3]???????????
	int numiCord = iCord.size();
	if (facePoints[4].y - facePoints[3].y > 0) {
		if ((iCord[2].x - facePoints[3].x) > double(facePoints[4].x - facePoints[3].x) / (facePoints[4].y - facePoints[3].y) * (iCord[2].y - facePoints[3].y)) {
			iCord[2].x = facePoints[3].x + double(facePoints[4].x - facePoints[3].x) / (facePoints[4].y - facePoints[3].y) * (iCord[2].y - facePoints[3].y);
		}
	}
	if (facePoints[12].y - facePoints[13].y > 0) {
		if ((facePoints[13].x - iCord[numiCord - 3].x) > double(facePoints[13].x - facePoints[12].x) / (facePoints[12].y - facePoints[13].y) * (iCord[numiCord - 3].y - facePoints[13].y)) {
			iCord[numiCord - 3].x = facePoints[13].x - double(facePoints[13].x - facePoints[12].x) / (facePoints[12].y - facePoints[13].y) * (iCord[numiCord - 3].y - facePoints[13].y);
		}
	}
}

//??mask??_??
void imageMatting_txt_model(Mat &imgOld, const Mat &maskOld, vector<Point> &facePoints, vector<Point> &iCord, string outFileName) {
	Mat mask, mask0, image;		//mask: Grabcut????????; mask0: ???????????; image: mask0?????????????Grabcut????
	cvtColor(maskOld, mask0, CV_BGR2GRAY);
	imgOld.copyTo(image, mask0);
	mask.create(image.size(), CV_8UC1);

	Mat res, binmask, showmask, showbinmask;	//res: ??Grabcut????; binmask: ?mask??0?1?; showmask?showbinmask: ????mask?binmask
	Mat bgdModel, fgdModel;

	int numi = iCord.size();
	Point p1(iCord[0]);			//??
	Point p2(iCord[numi - 1]);	//??
	Point p3(iCord[numi - 3]);	//??
	Point p4(iCord[numi - 2]);	//??
	int topY = max(0, p2.y);
	int bottomY = max(p1.y, p3.y);
	int leftX = double(p1.y - topY) * (p2.x - p1.x) / (p1.y - p2.y) + p1.x;
	int rightX = max(leftX + p3.x - p1.x, p3.x);
	leftX = min(p1.x, leftX);
	rightX = min(image.cols, rightX);

	Rect rect(leftX, topY, rightX - leftX + 1, bottomY - topY + 1);
	grabCut(image, mask, rect, bgdModel, fgdModel, 1, 0);

	//??Grabcut??
	/*showmask = mask.clone();
	showmask *= 80;
	getBinMask(mask, binmask);
	showbinmask = binmask.clone();
	showbinmask *= 80;
	res.release();
	image.copyTo(res, binmask);*/

	//??mask0????????????4???????????????????
	int leftEdgeX = p2.x + double(facePoints[0].y - p2.y) / (p1.y - p2.y) * (p1.x - p2.x);	//facePoint[0]???mask0?????X??
	int rightEdgeX = p4.x - double(facePoints[16].y - p4.y) / (p3.y - p4.y) * (p4.x - p3.x);	//facePoint[16]???mask0?????X??
	int diffLx, diffLy, diffRx, diffRy;
	diffLx = (facePoints[0].x - leftEdgeX) / 4;
	diffRx = (rightEdgeX - facePoints[16].x) / 4;
	//diffLy = iCord[1].y - iCord[0].y;
	//diffRy = iCord[numi - 3].y - iCord[numi - 4].y;
	p1.x += diffLx;
	//p1.y += diffLy;
	p2.x += diffLx;
	p3.x -= diffRx;
	//p3.y -= diffRy;
	p4.x -= diffRx;

	line(mask, p1, p2, GC_BGD, 3);
	/*showmask = mask.clone();
	showmask *= 80;*/
	line(mask, p3, p4, GC_BGD, 3);
	showmask = mask.clone();
	showmask *= 80;

	//??????
	line(mask, facePoints[17], facePoints[18], GC_FGD, 3);
	line(mask, facePoints[19], facePoints[22], GC_FGD, 3);
	line(mask, facePoints[23], facePoints[25], GC_FGD, 3);
	line(mask, facePoints[23], facePoints[26], GC_FGD, 3);
	line(mask, facePoints[25], facePoints[26], GC_FGD, 3);

	//0?1?2?14?15?16???????
	for (int i = 0; i < 3; i++) {
		line(mask, Point(facePoints[i].x / 2, facePoints[i].y), Point(0, facePoints[i].y), GC_BGD, 3);
		line(mask, Point((facePoints[16 - i].x + mask.cols) / 2, facePoints[16 - i].y), Point(mask.cols, facePoints[16 - i].y), GC_BGD, 3);
	}
	//0?16??????????
	line(mask, Point(facePoints[0].x / 2, facePoints[0].y / 2), Point(0, 0), GC_BGD, 3);
	line(mask, Point((facePoints[16].x + mask.cols) / 2, facePoints[16].y / 2), Point(mask.cols, 0), GC_BGD, 3);
	/*showmask = mask.clone();
	showmask *= 80;*/

	//??headPoints
	/*if (headPoints[1].y > 0){
	int yaxis = headPoints[1].y / 2;
	line(mask, Point(p2.x, yaxis), Point(p4.x, yaxis), GC_BGD);
	}*/

	//????headPoints
	/*int a = facePoints[8].x - facePoints[27].x, b = facePoints[8].y - facePoints[27].y;
	Point headTop(facePoints[8].x - 2.3 * a, facePoints[8].y - 2.3 * b);*/
	//???? ???
	int headtop = (2.3 * facePoints[23].y - 1.3 * facePoints[8].y) / 2;
	//???? ???
	//int headtop = (2 * facePoints[23].y - 1 * facePoints[8].y) / 2;

	line(mask, Point(0, headtop), Point(mask.cols, headtop), GC_BGD);


	cout << "Grabcut: ";
	clock_t start = clock();
	//??4???
	//for (int j = 0; j < 4; j++){
	//??1???
	for (int j = 0; j < 4; j++) {
		grabCut(image, mask, rect, bgdModel, fgdModel, 1, 1);
		cout << " " << j;
		//grabCut(image, mask, rect, bgdModel, fgdModel, 2, 2);
		/*showmask = mask.clone();
		showmask *= 80;
		getBinMask(mask, binmask);
		showbinmask = binmask.clone();
		showbinmask *= 80;
		res.release();
		image.copyTo(res, binmask);*/
	}
	clock_t end = clock();
	cout << " total time is: " << double(end - start) / 1000 << endl;

	//-----????mask??----------------------------------------------------------------------------------------------------------------------------------
	//?binmask?height??????????????????binmask???????
	getBinMask(mask, binmask);
	int height = max(iCord[0].y, iCord[numi - 3].y);
	double scale = 1.2;
	Mat adjmask = binmask(Rect(0, 0, binmask.cols, height));	//adjmask?binmask?height?????
	Size dsize(adjmask.cols * scale, adjmask.rows * scale);
	Mat tmp, newbin;
	Mat showadj, shownew, showbin, tmpbin, showtmp;
	resize(adjmask, tmp, dsize, 0, 0, CV_INTER_AREA);	//??dsize???tmp?
	adjmask = tmp(Rect((tmp.cols - adjmask.cols) / 2, (tmp.rows - adjmask.rows) / 2, adjmask.cols, adjmask.rows));	//?tmp??????adjmask?????
	newbin = adjmask | binmask(Rect(0, 0, binmask.cols, height));	//????adjmask????binmask?or???????adjmask????????binmask?????????
	newbin.copyTo(binmask(Rect(0, 0, binmask.cols, height)));	//???????mask????
																//showbin = binmask.clone();	//------------??????????????????copyto?????-------------
	newbin.release();
	binmask.copyTo(newbin, mask0);	//?mask0???????
									//shownew = newbin.clone();	//------------??????????????????copyto?????-------------
	binmask = newbin;
	//--------------------------------------------------------------------------------------------------------------------------------------------------

	//??mask??mask????
	//getBinMask(mask, binmask);
	Mat white(mask.size(), CV_8UC1);
	Mat mask2;
	white.setTo(255);
	white.copyTo(mask2, binmask);
	image.copyTo(res, binmask);
	imwrite(outFileName + ".bmp", res);
	imwrite(outFileName + "_3.bmp", mask2);
}

//??mask??_??
void imageMatting_txt_user(Mat &imgOld, const Mat &maskOld, vector<Point> &facePoints, vector<Point> &iCord, string outFileName) {
	Mat mask, mask0, image;		//mask: Grabcut????????; mask0: ???????????; image: mask0?????????????Grabcut????
	cvtColor(maskOld, mask0, CV_BGR2GRAY);
	imgOld.copyTo(image, mask0);

	Mat res, binmask, showmask, showbinmask;	//res: ??Grabcut????; binmask: ?mask??0?1?; showmask?showbinmask: ????mask?binmask
	Rect rect(6, 0, image.cols - 11, image.rows - 5);
	Mat bgdModel, fgdModel;

	grabCut(image, mask, rect, bgdModel, fgdModel, 1, 0);

	//??Grabcut??
	/*showmask = mask.clone();
	showmask *= 80;
	getBinMask(mask, binmask);
	showbinmask = binmask.clone();
	showbinmask *= 80;
	res.release();
	image.copyTo(res, binmask);*/

	//??mask0????????????4???????????????????
	int numi = iCord.size();
	/*Point p1(iCord[0].x, iCord[0].y);
	Point p2(iCord[numi - 3].x, iCord[numi - 3].y);
	Point p3(iCord[numi - 5].x, iCord[numi - 5].y);
	Point p4(iCord[numi - 4].x, iCord[numi - 4].y); */
	Point p1(iCord[0]);
	Point p2(iCord[numi - 1]);
	Point p3(iCord[numi - 3]);
	Point p4(iCord[numi - 2]);
	int leftEdgeX = p2.x + double(facePoints[0].y - p2.y) / (p1.y - p2.y) * (p1.x - p2.x);	//facePoint[0]???mask0?????X??
	int rightEdgeX = p4.x - double(facePoints[16].y - p4.y) / (p3.y - p4.y) * (p4.x - p3.x);	//facePoint[16]???mask0?????X??
	int diffLx, diffLy, diffRx, diffRy;
	diffLx = (facePoints[0].x - leftEdgeX) / 4;
	diffRx = (rightEdgeX - facePoints[16].x) / 4;
	//diffLy = iCord[1].y - iCord[0].y;
	//diffRy = iCord[numi - 3].y - iCord[numi - 4].y;
	p1.x += diffLx;
	//p1.y += diffLy;
	p2.x += diffLx;
	p3.x -= diffRx;
	//p3.y -= diffRy;
	p4.x -= diffRx;

	line(mask, p1, p2, GC_BGD, 3);
	/*showmask = mask.clone();
	showmask *= 80;*/
	line(mask, p3, p4, GC_BGD, 3);
	showmask = mask.clone();
	showmask *= 80;

	//??????
	line(mask, facePoints[17], facePoints[18], GC_FGD, 3);
	line(mask, facePoints[19], facePoints[22], GC_FGD, 3);
	line(mask, facePoints[23], facePoints[25], GC_FGD, 3);
	line(mask, facePoints[23], facePoints[26], GC_FGD, 3);
	line(mask, facePoints[25], facePoints[26], GC_FGD, 3);

	//0?1?2?14?15?16???????
	for (int i = 0; i < 3; i++) {
		line(mask, Point(facePoints[i].x / 2, facePoints[i].y), Point(0, facePoints[i].y), GC_BGD, 3);
		line(mask, Point((facePoints[16 - i].x + mask.cols) / 2, facePoints[16 - i].y), Point(mask.cols, facePoints[16 - i].y), GC_BGD, 3);
	}
	//0?16??????????
	line(mask, Point(facePoints[0].x / 2, facePoints[0].y / 2), Point(0, 0), GC_BGD, 3);
	line(mask, Point((facePoints[16].x + mask.cols) / 2, facePoints[16].y / 2), Point(mask.cols, 0), GC_BGD, 3);
	/*showmask = mask.clone();
	showmask *= 80;*/

	//??headPoints
	/*if (headPoints[1].y > 0){
	int yaxis = headPoints[1].y / 2;
	line(mask, Point(p2.x, yaxis), Point(p4.x, yaxis), GC_BGD);
	}*/

	//????headPoints
	/*int a = facePoints[8].x - facePoints[27].x, b = facePoints[8].y - facePoints[27].y;
	Point headTop(facePoints[8].x - 2.3 * a, facePoints[8].y - 2.3 * b);*/
	//???? ???
	//int headtop = (2.3 * facePoints[23].y - 1.3 * facePoints[8].y) / 2;
	//???? ???
	int headtop = (2 * facePoints[23].y - 1 * facePoints[8].y) / 2;

	line(mask, Point(0, headtop), Point(mask.cols, headtop), GC_BGD);

	/*showmask = mask.clone();
	showmask *= 80;*/

	cout << "Grabcut: ";
	//??4???
	//for (int j = 0; j < 4; j++){
	//??1???
	for (int j = 0; j < 4; j++) {
		grabCut(image, mask, rect, bgdModel, fgdModel, 1, 1);
		cout << " " << j;
		//grabCut(image, mask, rect, bgdModel, fgdModel, 2, 2);
		/*showmask = mask.clone();
		showmask *= 80;
		getBinMask(mask, binmask);
		showbinmask = binmask.clone();
		showbinmask *= 80;
		res.release();
		image.copyTo(res, binmask);*/
	}
	cout << endl;

	//??mask??mask????
	getBinMask(mask, binmask);
	Mat white(mask.size(), CV_8UC1);
	Mat mask2;
	white.setTo(255);
	white.copyTo(mask2, binmask);
	image.copyTo(res, binmask);
	imwrite(outFileName + ".bmp", res);
	imwrite(outFileName + "_3.bmp", mask2);
}

//??mask??_??
void imageMatting_txt_hair(const Mat &img_big, const Mat &img_small, const Mat &mask_small, vector<Point> &facePoints, vector<Point> &iCord, string outFileName) {
	int Thickness = 6;
	Mat mask, mask0, image;		//mask: Grabcut????????; mask0: ???????????;
	Mat res, binmask, showmask, showbinmask;	//res: ??Grabcut????; binmask: ?mask??0?1?; showmask?showbinmask: ????mask?binmask
	cvtColor(mask_small, mask0, CV_BGR2GRAY);
	img_small.copyTo(image);	//??????
								//img_small.copyTo(image, mask0);	//???????

	mask.create(mask0.size(), CV_8UC1);
	for (int i = 0; i < mask.rows; i++) {
		for (int j = 0; j < mask.cols; j++) {
			mask.at<uchar>(i, j) = (mask0.at<uchar>(i, j) == 0 ? GC_PR_BGD : GC_PR_FGD);
		}
	}
	//mask.setTo(Scalar(GC_PR_FGD));

	int numi = iCord.size();
	Point p1(iCord[0]);			//??
	Point p2(iCord[numi - 1]);	//??
	Point p3(iCord[numi - 3]);	//??
	Point p4(iCord[numi - 2]);	//??
	int topY = max(0, p2.y);
	int bottomY = max(p1.y, p3.y);
	int leftX = double(p1.y - topY) * (p2.x - p1.x) / (p1.y - p2.y) + p1.x;
	int rightX = max(leftX + p3.x - p1.x, p3.x);
	leftX = max(0, min(p1.x, leftX));
	rightX = min(image.cols - 1, rightX);

	Rect rect(leftX, topY, rightX - leftX + 1, bottomY - topY + 1);
	Mat bgdModel, fgdModel;

	//??????
	vector<Point> leftRegion, rightRegion;
	leftRegion.push_back(p1);
	leftRegion.push_back(p2);
	leftRegion.push_back(Point(0, p2.y));
	leftRegion.push_back(Point(0, p1.y));
	rightRegion.push_back(p3);
	rightRegion.push_back(p4);
	rightRegion.push_back(Point(mask.cols, p4.y));
	rightRegion.push_back(Point(mask.cols, p3.y));
	vector<vector<Point>> pol;
	pol.push_back(leftRegion);
	pol.push_back(rightRegion);
	fillPoly(mask, pol, GC_BGD);

	//??mask0????????????4???????????????????
	int leftEdgeX = p2.x + double(facePoints[0].y - p2.y) / (p1.y - p2.y) * (p1.x - p2.x);	//facePoint[0]???mask0?????X??
	int rightEdgeX = p4.x - double(facePoints[16].y - p4.y) / (p3.y - p4.y) * (p4.x - p3.x);	//facePoint[16]???mask0?????X??
	int diffLx, diffLy, diffRx, diffRy;
	diffLx = (facePoints[0].x - leftEdgeX) / 4;
	diffRx = (rightEdgeX - facePoints[16].x) / 4;
	//diffLy = iCord[1].y - iCord[0].y;
	//diffRy = iCord[numi - 3].y - iCord[numi - 4].y;
	p1.x += diffLx;
	//p1.y += diffLy;
	p2.x += diffLx;
	p3.x -= diffRx;
	//p3.y -= diffRy;
	p4.x -= diffRx;

	line(mask, p1, p2, GC_BGD, Thickness);
	/*showmask = mask.clone();
	showmask *= 80;*/
	line(mask, p3, p4, GC_BGD, Thickness);

	//??????
	line(mask, facePoints[17], facePoints[18], GC_FGD, Thickness);
	line(mask, facePoints[19], facePoints[22], GC_FGD, Thickness);
	line(mask, facePoints[23], facePoints[25], GC_FGD, Thickness);
	line(mask, facePoints[23], facePoints[26], GC_FGD, Thickness);
	line(mask, facePoints[25], facePoints[26], GC_FGD, Thickness);

	//0?1?2?14?15?16???????
	for (int i = 0; i < 3; i++) {
		line(mask, Point(facePoints[i].x / 2, facePoints[i].y), Point(0, facePoints[i].y), GC_BGD, Thickness);
		line(mask, Point((facePoints[16 - i].x + mask.cols) / 2, facePoints[16 - i].y), Point(mask.cols, facePoints[16 - i].y), GC_BGD, Thickness);
	}
	//0?16??????????
	line(mask, Point(facePoints[0].x / 2, facePoints[0].y / 2), Point(0, 0), GC_BGD, Thickness);
	line(mask, Point((facePoints[16].x + mask.cols) / 2, facePoints[16].y / 2), Point(mask.cols, 0), GC_BGD, Thickness);

	//??headPoints
	/*if (headPoints[1].y > 0){
	int yaxis = headPoints[1].y / 2;
	line(mask, Point(p2.x, yaxis), Point(p4.x, yaxis), GC_BGD);
	}*/

	//????headPoints
	/*int a = facePoints[8].x - facePoints[27].x, b = facePoints[8].y - facePoints[27].y;
	Point headTop(facePoints[8].x - 2.3 * a, facePoints[8].y - 2.3 * b);*/
	//???? ???
	//int headtop = (2.3 * facePoints[23].y - 1.3 * facePoints[8].y) / 2;
	//???? ???
	int headtop = (2 * facePoints[23].y - 1 * facePoints[8].y) / 2;
	line(mask, Point(0, headtop), Point(mask.cols, headtop), GC_PR_BGD, Thickness);

	int count = 1;
	clock_t start = clock();
	grabCut(image, mask, rect, bgdModel, fgdModel, 1, 1);
	clock_t end = clock();
	cout << "grabcut: " << count++ << ", time: " << double(end - start) / CLOCKS_PER_SEC << endl;

	getBinMask(mask, binmask);
	res = image.clone();
	for (int i = 0; i < binmask.rows; i++) {
		for (int j = 0; j < binmask.cols; j++) {
			res.at<Vec3b>(i, j) -= (binmask.at<uchar>(i, j) == 0 ? Vec3b(90, 90, 0) : Vec3b(0, 0, 0));
		}
	}

	//????
	string winName = "image";
	namedWindow(winName, WINDOW_AUTOSIZE);
	setMouseCallback(winName, on_mouse, 0);

	hairMarker.setImageAndWinName(res, winName);
	hairMarker.showImage();
	help();

	bool saved = false;
	for (;;)
	{
		if (saved)
			break;
		int c = waitKey(0);
		switch ((char)c)
		{
		case 's':
			cout << "Saving and exiting ..." << endl;
			saved = true;
			break;
		case 'r':
			cout << endl;
			hairMarker.reset();
			hairMarker.showImage();
			break;
		case 'n':
			/*int iterCount = hairMarker.getIterCount();
			cout << "<" << iterCount << "... ";
			int newIterCount = hairMarker.nextIter();
			if (newIterCount > iterCount)
			{
			hairMarker.showImage();
			cout << iterCount << ">" << endl;
			}
			else
			cout << "rect must be determined>" << endl;*/
			saveLabel(hairMarker, mask, outFileName, false);
			start = clock();
			grabCut(image, mask, rect, bgdModel, fgdModel, 1, 1);
			end = clock();
			cout << "grabcut: " << count++ << ", time: " << double(end - start) / CLOCKS_PER_SEC << endl;

			getBinMask(mask, binmask);
			res = image.clone();
			for (int i = 0; i < binmask.rows; i++) {
				for (int j = 0; j < binmask.cols; j++) {
					res.at<Vec3b>(i, j) -= (binmask.at<uchar>(i, j) == 0 ? Vec3b(90, 90, 0) : Vec3b(0, 0, 0));
				}
			}
			hairMarker.setImageAndWinName(res, winName);
			hairMarker.showImage();

			break;
		}
	}
	cout << endl;

	//-----????mask????????????????----------------------------------------------------------------------------------------------------------------------------------
	//?binmask?height??????????????????binmask???????
	getBinMask(mask, binmask);
	res.release();
	image.copyTo(res, binmask);
	int height = max(iCord[0].y, iCord[numi - 3].y);
	double scale = 0.98;
	Mat adjmask = binmask(Rect(0, 0, binmask.cols, height));	//adjmask?binmask?height?????
	Size dsize(adjmask.cols * scale, adjmask.rows * scale);
	Mat tmp, newbin;
	Mat showadj, shownew, showbin, tmpbin, showtmp;
	resize(adjmask, tmp, dsize, 0, 0, CV_INTER_AREA);	//??dsize???tmp?
	tmp.copyTo(binmask(Rect((adjmask.cols - tmp.cols) / 2, adjmask.rows - tmp.rows, tmp.cols, tmp.rows)));
	res.release();
	image.copyTo(res, binmask);
	//--------------------------------------------------------------------------------------------------------------------------------------------------

	//??mask??mask????
	//getBinMask(mask, binmask);
	//?binmask??
	Size dsize1(img_big.cols, img_big.rows);
	Mat binmask_big;
	resize(binmask, binmask_big, dsize1, 0, 0, CV_INTER_AREA);

	Mat white(img_big.size(), CV_8UC1);
	Mat mask2;
	white.setTo(255);
	white.copyTo(mask2, binmask_big);
	res.release();
	img_big.copyTo(res, binmask_big);
	imwrite(outFileName + ".bmp", res);
	imwrite(outFileName + "_3.bmp", mask2);
}



// xml??
void drawShoulder_xml(string filePre, string xmlName, string outPre)
{
	HRESULT hr = S_OK;

	IFaceSdkInterface*      detector = nullptr;
	IFaceSdkInterface*      alignmentor = nullptr;
	UINT                    landmarkCount = 0;

	FaceSdkInterfaceType    detectorType = FaceSdkInterfaceType::FaceDetection_Multiview;
	FaceSdkInterfaceType    alignmentType = FaceSdkInterfaceType::FaceAlignment_68_Points_LBF;

	if (SUCCEEDED(hr))
	{
		// Initialize COM apartment for WIC
		hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	}

	if (SUCCEEDED(hr))
	{
		hr = CreateDetectorInterface(detectorType, &detector);
	}

	if (SUCCEEDED(hr))
	{
		hr = CreateAlignmentorInterface(alignmentType, &alignmentor);
	}

	if (SUCCEEDED(hr))
	{
		hr = GetLandmarkCount(alignmentor, &landmarkCount);
	}

	// xml parse
	TiXmlDocument *pDoc = new TiXmlDocument();
	pDoc->LoadFile(xmlName.c_str());
	TiXmlElement *pRoot = pDoc->RootElement();
	TiXmlElement *pPic = pRoot->FirstChildElement();
	ofstream fout;
	fout.open(outPre + "height.txt");
	while (pPic != NULL)
	{
		TiXmlAttribute *pName = pPic->FirstAttribute();
		string fName(pName->Value());
		string fileName = filePre + fName;
		string outName = outPre + fName;
		cout << fileName << " to " << outName << endl;
		Mat img = imread(fileName);
		//namedWindow("image", WINDOW_AUTOSIZE);
		// xml point
		// read the six cordinate
		TiXmlElement *pPoint = pPic->FirstChildElement();
		double cordinate[9][2];
		int nCord = 0;
		while (pPoint != NULL)
		{
			TiXmlAttribute *pAttr = pPoint->FirstAttribute();
			int nDim = 0;
			while (pAttr != NULL)
			{
				//cout << pAttr->Name() << "=" << pAttr->Value() << "   ";
				cordinate[nCord][nDim] = atof(pAttr->Value());
				nDim++;
				pAttr = pAttr->Next();
			}
			nCord++;
			//cout << endl;
			pPoint = pPoint->NextSiblingElement();
		}

		//waitKey();

		vector<Point> iCord;	// the points including shoulder, chin, square
		vector<Point> sCord;	// only square
		vector<Point> headPoints;	//??3??
		for (int i = 0; i < 6; i++)
		{
			Point iPoint = Point((int)(cordinate[i][0] * img.cols), (int)(cordinate[i][1] * img.rows));
			iCord.push_back(iPoint);
			//cout << iCord[i][0] << " " << " " << iCord[i][1] << endl;
		}
		for (int i = 6; i < 9; i++)
		{
			Point iPoint = Point((int)(cordinate[i][0] * img.cols), (int)(cordinate[i][1] * img.rows));
			headPoints.push_back(iPoint);
			//cout << iCord[i][0] << " " << " " << iCord[i][1] << endl;
		}
		// save the min height of the shoulder
		int minHeight = max(iCord[0].y, iCord[5].y);
		for (int i = 0; i < fName.length() - 4; i++)
			fout << fName[i];
		fout << "\t" << minHeight << endl;

		// insert chin points to iCord
		vector<Point> facePoints;
		insertChin(iCord, fileName, detector, alignmentor, landmarkCount, facePoints);

		// find the upper square
		getMask(iCord, sCord, facePoints);

		ofstream fout_point;
		int outNameLen = outName.length();
		outName[outNameLen - 3] = 't';
		outName[outNameLen - 2] = 'x';
		outName[outNameLen - 1] = 't';
		fout_point.open(outName);
		int numi = iCord.size();
		//???????????
		fout_point << iCord[0].x << " " << iCord[0].y << endl
			<< iCord[numi - 1].x << " " << iCord[numi - 1].y << endl
			<< iCord[numi - 3].x << " " << iCord[numi - 3].y << endl
			<< iCord[numi - 2].x << " " << iCord[numi - 2].y << endl;
		if (!facePoints.empty()) {
			for (int i = 0; i < facePoints.size(); i++) {
				fout_point << facePoints[i].x << " " << facePoints[i].y << endl;
			}
		}
		fout_point.close();
		// draw the lines
		Mat imgOld = img.clone();
		for (int i = 0; i < iCord.size() - 1; i++)
		{
			Point st = Point(iCord[i].x, iCord[i].y), ed = Point(iCord[i + 1].x, iCord[i + 1].y);
			line(img, st, ed, Scalar(0, 0, 255), 2);
		}
		/*
		vector<Point> resPoint;
		approxPolyDP(iCord, resPoint, 0.1, false);
		for (int i = 0; i < resPoint.size() - 1; i++)
		{
		line(img, resPoint[i], resPoint[i + 1], Scalar(0, 0, 255), 2);
		}
		*/

		vector<vector<Point>> polys;
		//iCord.pop_back();
		//polys.push_back(sCord);
		polys.push_back(sCord);
		// set to white
		img.setTo(Scalar(0, 0, 0));
		string outMaskName(outName);
		outMaskName.insert(outMaskName.length() - 4, "_1");
		int outMaskLen = outMaskName.length();
		outMaskName[outMaskLen - 3] = 'b';
		outMaskName[outMaskLen - 2] = 'm';
		outMaskName[outMaskLen - 1] = 'p';
		fillPoly(img, polys, Scalar(255, 255, 255));
		imwrite(outMaskName, img);
		polys.clear();
		img.setTo(Scalar(0, 0, 0));
		polys.push_back(iCord);
		fillPoly(img, polys, Scalar(255, 255, 255));
		outMaskName[outMaskLen - 5] = '2';
		imwrite(outMaskName, img);
		//fillConvexPoly(img, iCord, Scalar(255, 255, 255));
		imageMatting_txt_user(imgOld, img, facePoints, iCord, outName);


		pPic = pPic->NextSiblingElement();


	}
	fout.close();
}

// txt??
void drawShoulder_txt(string filePre, string picInfo, string outPre)
{
	//clear the outPre directory
	//system("exec rm -r E:\FashionRoom\test\output\model2\pics\*");
	// ??picture information
	ifstream fin(picInfo, ios::_Nocreate);

	ofstream fout;

	fout.open(outPre + "height.txt");	//??shoulder?????

	if (!fin)
		return;

	while (!fin.eof()) {
		//?????key points?pose??
		string fileName;
		vector<Point> facePoints(27);	//0~16, 19, 24, 36, 39, 42, 45, 27, 33, 48, 54	??
		vector<Point> facePoints_small(27);	//0~16, 19, 24, 36, 39, 42, 45, 27, 33, 48, 54	??
		vector<Point> shoulderPoints(6);	//?~?	??
		vector<Point> shoulderPoints_small(6);	//?~?	??
		double poseX, poseY, poseZ;
		double x, y;
		fin >> fileName;
		if (fileName == "") {
			break;
		}
		Mat img = imread(filePre + fileName);

		string outFileName;
		if (fileName[fileName.length() - 5] == '.')
			outFileName = outPre + fileName.substr(0, fileName.length() - 5);	// "xxx.jpeg" ->: "E:\\aa\\bb\\xxx"
		else
			outFileName = outPre + fileName.substr(0, fileName.length() - 4);	// "xxx.jpg" ->: "E:\\aa\\bb\\xxx"

																				/*
																				----------?????picWidth_big???mask??????? picWidth_small????????------------
																				*/
		const int picWidth_big = 250;
		const int picWidth_small = 250;
		Mat img_big, img_small;
		if (img.cols > picWidth_big) {
			double scale = (double)picWidth_big / img.cols;
			Size dsize = Size(img.cols * scale, img.rows * scale);
			resize(img, img_big, dsize, 0, 0, CV_INTER_AREA);

			//????????
			imwrite(outFileName + ".jpg", img_big);

			scale = (double)picWidth_small / img.cols;
			dsize = Size(img.cols * scale, img.rows * scale);
			resize(img, img_small, dsize, 0, 0, CV_INTER_AREA);
		}
		else if (img.cols > picWidth_small) {
			img_big = img;
			imwrite(outFileName + ".jpg", img);
			double scale = (double)picWidth_small / img.cols;
			Size dsize = Size(img.cols * scale, img.rows * scale);
			resize(img, img_small, dsize, 0, 0, CV_INTER_AREA);
		}
		else {
			img_big = img;
			imwrite(outFileName + ".jpg", img);
			img_small = img;
		}
		//--------???? --------------------------------

		cout << "processing" + filePre + fileName << endl;
		for (int i = 0; i < 27; i++) {
			fin >> x >> y;
			facePoints[i] = Point(img_big.cols * x, img_big.rows * y);
			facePoints_small[i] = Point(img_small.cols * x, img_small.rows * y);
		}
		for (int i = 0; i < 6; i++) {
			fin >> x >> y;
			shoulderPoints[i] = Point(img_big.cols * x, img_big.rows * y);
			shoulderPoints_small[i] = Point(img_small.cols * x, img_small.rows * y);
		}
		fin >> poseX >> poseY >> poseZ;

		// save the min height of the shoulder
		int minHeight = max(shoulderPoints[0].y, shoulderPoints[5].y);
		for (int i = 0; i < fileName.length(); i++) {
			if (fileName[i] == '.')
				break;
			fout << fileName[i];
		}


		fout << "\t" << minHeight << endl;

		//???iCord???+??????
		vector<Point> iCord, iCord_small, sCord, sCord_small;
		for (int i = 0; i < 3; i++) {
			iCord.push_back(shoulderPoints[i]);
			iCord_small.push_back(shoulderPoints_small[i]);
		}
		for (int i = 3; i < 14; i++) {
			iCord.push_back(facePoints[i]);
			iCord_small.push_back(facePoints_small[i]);
		}
		for (int i = 3; i < 6; i++) {
			iCord.push_back(shoulderPoints[i]);
			iCord_small.push_back(shoulderPoints_small[i]);
		}

		//????????: iCord[2]??facePoints[3]?facePoints[4]????; iCord[num - 3]??facePoints[13]?facePoints[12]????
		//??iCord[2]?iCord[num - 3]???????????
		adjShoulder(iCord, facePoints);
		adjShoulder(iCord_small, facePoints_small);

		// find the upper square ????&??????????mask???????
		getMask_txt(iCord, sCord, facePoints);
		getMask_txt(iCord_small, sCord_small, facePoints_small);

		// draw the lines
		Mat mask_big, mask_small;
		mask_big.create(img_big.size(), CV_8UC3);
		mask_small.create(img_small.size(), CV_8UC3);
		/*for (int i = 0; i < iCord.size() - 1; i++)
		{
		Point st = Point(iCord[i].x, iCord[i].y), ed = Point(iCord[i + 1].x, iCord[i + 1].y);
		line(img, st, ed, Scalar(0, 0, 255), 2);
		}*/

		vector<vector<Point>> polys;
		polys.push_back(sCord);
		// set to white
		mask_big.setTo(Scalar(0, 0, 0));
		fillPoly(mask_big, polys, Scalar(255, 255, 255));	//poly?mask????
		imwrite(outFileName + "_1.bmp", mask_big);
		polys.clear();
		mask_big.setTo(Scalar(0, 0, 0));
		polys.push_back(iCord);
		fillPoly(mask_big, polys, Scalar(255, 255, 255));
		imwrite(outFileName + "_2.bmp", mask_big);

		polys.clear();
		mask_small.setTo(Scalar(0, 0, 0));
		polys.push_back(iCord_small);
		fillPoly(mask_small, polys, Scalar(255, 255, 255));
		//????(?????)
		//imageMatting_txt_hair(img_big, img_small, mask_small, facePoints_small, iCord_small, outFileName);
		//????
		imageMatting_txt_model(img_big, mask_big, facePoints, iCord, outFileName);
		//????(??)
		//imageMatting_txt_user(img_big, mask_big, facePoints, iCord, outFileName);
	}
	fout.close();
}

int main()
{
	//string filePre = "E:\\FashionRoom\\test\\origin\\model\\pics\\";
	//string picInfo = "E:\\FashionRoom\\test\\origin\\model\\picInfo.txt";
	//string outPre = "E:\\FashionRoom\\test\\output\\model\\pics\\";

	string filePre = "E:\\tupian\\str2int\\";
	string picInfo = "E:\\tupian\\grabcut\\picInfo.txt";
	string outPre = "E:\\tupian\\grabcut\\pics\\";

	//string filePre = "C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\user1\\pics\\";
	//string picInfo = "C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\origin\\user1\\picInfo.txt";
	//string outPre = "C:\\Users\\v-yuhyua\\Desktop\\FashionRoom\\test\\output\\user1\\pics\\";
	cout << "start" << endl;
	//drawShoulder_xml(filePre, "facePoint.txt", outPre);
	drawShoulder_txt(filePre, picInfo, outPre);
	//FileStorage fs;
	//fs.open(xmlName, FileStorage::READ);
	//imshow("image", img);
	//waitKey();
	system("pause");
	return 0;
}
