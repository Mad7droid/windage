/* ========================================================================
 * PROJECT: windage Library
 * ========================================================================
 * This work is based on the original windage Library developed by
 *   Woonhyuk Baek (wbaek@gist.ac.kr / windage@live.com)
 *   Woontack Woo (wwoo@gist.ac.kr)
 *   U-VR Lab, GIST of Gwangju in Korea.
 *   http://windage.googlecode.com/
 *   http://uvr.gist.ac.kr/
 *
 * Copyright of the derived and new portions of this work
 *     (C) 2009 GIST U-VR Lab.
 *
 * This framework is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this framework; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * For further information please contact 
 *   Woonhyuk Baek
 *   <windage@live.com>
 *   GIST U-VR Lab.
 *   Department of Information and Communication
 *   Gwangju Institute of Science and Technology
 *   1, Oryong-dong, Buk-gu, Gwangju
 *   South Korea
 * ========================================================================
 ** @author   Woonhyuk Baek
 * ======================================================================== */

#include <iostream>

#include <cv.h>
#include <highgui.h>

#include <windage.h>
#include "../Common/FleaCamera.h"

const int WIDTH = 640;
const int HEIGHT = (WIDTH * 3) / 4;
const double INTRINSIC[] = {1033.93, 1033.84, 319.044, 228.858,-0.206477, 0.306424, 0.000728208, 0.0011338};

#define ADAPTIVE_THRESHOLD 1
const int FEATURE_COUNT = WIDTH*2;

const double SCALE_FACTOR = 6.0;
const int SCALE_STEP = 6;
const double REPROJECTION_ERROR = 5.0;

#define USE_TEMPLATE_IMAEG 1
const char* TEMPLATE_IMAGE = "reference%d_320.png";
const int TEMPLATE_IMAGE_COUNT = 2;

const char* FEATURE_FILE_TEMPLATE = "demo_FEATURE/scene_%.4d.bmp";
const char* DESCRIPTOR_FILE_TEMPLATE = "demo_DESCRIPTOR/scene_%.4d.bmp";
const char* MATCHING_FILE_TEMPLATE = "demo_MATCHING/scene_%.4d.bmp";
const char* RESULT_FILE_TEMPLATE = "demo_RESULT/scene_%.4d.bmp";
const char* INPUT_IMAGE_TEMPLATE = "demo_IMAGE/flycap%d.jpg";

#define USING_CAMERA 1
#define USING_IMAGE_SEQ 0

void main()
{
	char filename[1000];
	windage::Logger logger(&std::cout);

	IplImage* inputImage;
	IplImage* resizeImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	IplImage* grayImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 1);
	IplImage* resultImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);

#if USING_CAMERA
	FleaCamera* capture = new FleaCamera();
	capture->open();
	capture->start();
	//CvCapture* capture = cvCaptureFromCAM(CV_CAP_ANY);
#endif

	IplImage* featureImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	IplImage* descriptorImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	IplImage* matchingImage = cvCreateImage(cvSize(WIDTH*2, HEIGHT*2), IPL_DEPTH_8U, 3);
	IplImage* matchingImage2 = cvCreateImage(cvSize(WIDTH*2, HEIGHT), IPL_DEPTH_8U, 3);
	IplImage* matchingResult = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	cvNamedWindow("FEATURE");
	cvNamedWindow("DESCRIPTOR");
	cvNamedWindow("MATCHING");
	cvNamedWindow("result");

	// create and initialize tracker
	double threshold = 50.0;

	windage::Frameworks::MultiplePlanarObjectTracking tracking;
	windage::Calibration* calibration;
	windage::Algorithms::FeatureDetector* detector;
	windage::Algorithms::OpticalFlow* opticalflow;
	windage::Algorithms::HomographyEstimator* estimator;
	windage::Algorithms::OutlierChecker* checker;
	windage::Algorithms::HomographyRefiner* refiner;

	calibration = new windage::Calibration();
	detector = new windage::Algorithms::WSURFdetector();
	opticalflow = new windage::Algorithms::OpticalFlow();
	estimator = new windage::Algorithms::ProSACestimator();
	checker = new windage::Algorithms::OutlierChecker();
	refiner = new windage::Algorithms::LMmethod();

	calibration->Initialize(INTRINSIC[0], INTRINSIC[1], INTRINSIC[2], INTRINSIC[3], INTRINSIC[4], INTRINSIC[5], INTRINSIC[6], INTRINSIC[7]);
	detector->SetThreshold(50.0);
	opticalflow->Initialize(WIDTH, HEIGHT, cvSize(15, 15), 3);
	estimator->SetReprojectionError(REPROJECTION_ERROR);
	checker->SetReprojectionError(REPROJECTION_ERROR * 3);
	refiner->SetMaxIteration(10);

	tracking.AttatchCalibration(calibration);
	tracking.AttatchDetetor(detector);
	tracking.AttatchTracker(opticalflow);
	tracking.AttatchEstimator(estimator);
	tracking.AttatchChecker(checker);
	tracking.AttatchRefiner(refiner);
	
	tracking.Initialize(WIDTH, HEIGHT, (double)WIDTH, (double)HEIGHT);
	tracking.SetFilter(false);
	tracking.SetDitectionRatio(0);

	bool trained = false;
#if USE_TEMPLATE_IMAEG
	for(int i=0; i<TEMPLATE_IMAGE_COUNT; i++)
	{
		char message[100];
		sprintf_s(message, TEMPLATE_IMAGE, i+1);

		IplImage* colorSample = cvLoadImage(message);
		IplImage* sampleImage = cvLoadImage(message, 0);
		detector->SetThreshold(30.0);
		tracking.AttatchReferenceImage(sampleImage);

		cvSetImageROI(matchingImage, cvRect(WIDTH*i, HEIGHT, WIDTH, HEIGHT));
		cvResize(colorSample, matchingImage);

		cvReleaseImage(&sampleImage);
		cvReleaseImage(&colorSample);
	}
	cvSetImageROI(matchingImage, cvRect(0, HEIGHT, WIDTH*2, HEIGHT));
	cvCopyImage(matchingImage, matchingImage2);

	tracking.TrainingReference(SCALE_FACTOR, SCALE_STEP);
	trained = true;
	detector->SetThreshold(threshold);
#endif

	int keypointCount = 0;
	int matchingCount = 0;
	double processingTime = 0.0;

	int index = 0;

	char message[100];
	bool fliping = true;
	bool processing = true;
	while(processing)
	{
#if USING_CAMERA
		capture->update();
		inputImage = capture->GetIPLImage();
		cvCvtColor(inputImage, resizeImage, CV_BGRA2BGR);
#endif
#if USING_IMAGE_SEQ
		sprintf(filename, INPUT_IMAGE_TEMPLATE, index);
		inputImage = cvLoadImage(filename);
		cvCopyImage(inputImage, resizeImage);		
		cvReleaseImage(&inputImage);
#endif
/*
		inputImage = cvRetrieveFrame(capture);
		cvResize(inputImage, resizeImage);
		if(fliping)
			cvFlip(resizeImage, resizeImage);
*/
		// for procedure information
		cvCopyImage(resizeImage, featureImage);
		cvCopyImage(resizeImage, descriptorImage);

		cvSetImageROI(matchingImage, cvRect(0, 0, WIDTH, HEIGHT));
		cvCopyImage(resizeImage, matchingImage);
		cvSetImageROI(matchingImage, cvRect(WIDTH, 0, WIDTH, HEIGHT));
		cvCopyImage(resizeImage, matchingImage);
		cvSetImageROI(matchingImage, cvRect(0, HEIGHT, WIDTH*2, HEIGHT));
		cvCopyImage(matchingImage2, matchingImage);
		cvResetImageROI(matchingImage);

		cvCvtColor(resizeImage, grayImage, CV_BGR2GRAY);
		cvCopyImage(resizeImage, resultImage);

		logger.updateTickCount();

		// track object
		if(trained)
		{
			tracking.UpdateCamerapose(grayImage);
//			tracking.GetDetector()->DrawKeypoints(resultImage);

			// adaptive threshold
#if ADAPTIVE_THRESHOLD
			int localcount = detector->GetKeypointsCount();
			if(keypointCount != localcount)
			{
				if(localcount > FEATURE_COUNT)
					threshold += 1;
				if(localcount < FEATURE_COUNT)
					threshold -= 1;
				detector->SetThreshold(threshold);
				keypointCount = localcount;
			}
#endif
			// draw result
			std::vector<int> matchingCount; matchingCount.resize(tracking.GetObjectCount());
			for(int i=0; i<tracking.GetObjectCount(); i++)
			{
				matchingCount[i] = tracking.GetMatchingCount(i);
				if(tracking.GetMatchingCount(i) > 10)
				{
//					tracking.DrawDebugInfo(resultImage, i);
					tracking.DrawOutLine(resultImage, i, true);
					windage::Calibration* calibrationTemp = tracking.GetCameraParameter(i);
					calibrationTemp->DrawInfomation(resultImage, 100);
					CvPoint centerPoint = calibrationTemp->ConvertWorld2Image(0.0, 0.0, 0.0);
					
					centerPoint.x += 5;
					centerPoint.y += 10;
					sprintf_s(message, "object #%d (%03d)", i+1, matchingCount[i]);
					windage::Utils::DrawTextToImage(resultImage, centerPoint, 0.6, message);
				}
			}
		}

		processingTime = logger.calculateProcessTime();
		logger.log("processingTime", processingTime);
		logger.logNewLine();

		cvShowImage("result", resultImage);

		// for procedure information
		{
			std::vector<windage::FeaturePoint>* features = detector->GetKeypoints();
			for(int i=0; i<features->size(); i++)
			{
				cvCircle(featureImage, cvPoint((*features)[i].GetPoint().x, (*features)[i].GetPoint().y), 10, CV_RGB(255, 0, 0), 2);
			}

			detector->DrawKeypoints(descriptorImage, CV_RGB(0, 0, 255));

			cvSetImageROI(matchingImage, cvRect(WIDTH*0, 0, WIDTH, HEIGHT*2));
			tracking.DrawDebugInfo2(matchingImage, 0);
			cvSetImageROI(matchingImage, cvRect(WIDTH*1, 0, WIDTH, HEIGHT*2));
			tracking.DrawDebugInfo2(matchingImage, 1);
			cvResetImageROI(matchingImage);
			cvResize(matchingImage, matchingResult);

			cvShowImage("FEATURE", featureImage);
			cvShowImage("DESCRIPTOR", descriptorImage);
			cvShowImage("MATCHING", matchingResult);

			
			sprintf(filename, FEATURE_FILE_TEMPLATE, index);
			cvSaveImage(filename, featureImage);
			sprintf(filename, DESCRIPTOR_FILE_TEMPLATE, index);
			cvSaveImage(filename, descriptorImage);
			sprintf(filename, MATCHING_FILE_TEMPLATE, index);
			cvSaveImage(filename, matchingResult);
			sprintf(filename, RESULT_FILE_TEMPLATE, index);
			cvSaveImage(filename, resultImage);
			index++;
		}

		char ch = cvWaitKey(1);
		switch(ch)
		{
		case 'q':
		case 'Q':
			processing = false;
			break;
		case 'f':
		case 'F':
			fliping = !fliping;
			break;
		case ' ':
		case 's':
		case 'S':
			detector->SetThreshold(30.0);
			tracking.AttatchReferenceImage(grayImage);
			tracking.TrainingReference(SCALE_FACTOR, SCALE_STEP);
			detector->SetThreshold(threshold);
			trained = true;
			break;
		}		
	}

	//	cvReleaseCapture(&capture);
#if USING_CAMERA
	capture->stop();
	capture->close();
#endif

	cvDestroyAllWindows();
}
