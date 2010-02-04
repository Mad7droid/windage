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

#include "Frameworks/MultiplePlanarObjectTracking.h"
using namespace windage;
using namespace windage::Frameworks;

bool MultiplePlanarObjectTracking::Initialize(int width, int height, double realWidth, double realHeight)
{
	if(this->initialCamearParameter == NULL)
		return false;
	
	this->width = width;
	this->height = height;
	this->realWidth = realWidth;
	this->realHeight = realHeight;

	this->checker->AttatchEstimator(this->estimator);
	
	if(prevImage) cvReleaseImage(&prevImage);
	prevImage = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);

	this->initialize = true;
	return true;
}

bool MultiplePlanarObjectTracking::AttatchReferenceImage(IplImage* grayImage)
{
	if(grayImage == NULL)
		return false;

	this->referenceImage.push_back(cvCloneImage(grayImage));
	this->objectCount++;

	this->cameraParameter.resize(this->objectCount);
	this->cameraParameter[this->objectCount-1] = new windage::Calibration();
	this->cameraParameter[this->objectCount-1]->Initialize(	this->initialCamearParameter->GetParameters()[0],
															this->initialCamearParameter->GetParameters()[1],
															this->initialCamearParameter->GetParameters()[2],
															this->initialCamearParameter->GetParameters()[3],
															this->initialCamearParameter->GetParameters()[4],
															this->initialCamearParameter->GetParameters()[5],
															this->initialCamearParameter->GetParameters()[6],
															this->initialCamearParameter->GetParameters()[7]);

	this->referenceRepository.resize(this->objectCount);
	this->refMatchedKeypoints.resize(this->objectCount);
	this->sceMatchedKeypoints.resize(this->objectCount);
	this->searchTree.resize(this->objectCount);
	this->searchTree[this->objectCount-1] = new SearchTreeT();

	this->detectionRatio = this->objectCount;

	return true;
}

bool MultiplePlanarObjectTracking::TrainingReference(double scaleFactor, int scaleStep)
{
	if(this->objectCount <= 0)
		return false;

	int width = cvRound((double)this->referenceImage[this->objectCount-1]->width/scaleFactor);
	int height = cvRound((double)this->referenceImage[this->objectCount-1]->height/scaleFactor);

	for(int i=0; i<objectCount; i++)
	{
		this->referenceRepository[i].clear();

		int count = 0;
		for(int y=1; y<=scaleStep; y++)
		{
			for(int x=1; x<=scaleStep; x++)
			{
				IplImage* resizeReferenceImage = cvCreateImage(cvSize(width*x, height*y), IPL_DEPTH_8U, 1);
				cvResize(this->referenceImage[i], resizeReferenceImage, CV_INTER_LINEAR);
				cvSmooth(resizeReferenceImage, resizeReferenceImage, CV_GAUSSIAN, 3, 3);

				this->detector->DoExtractKeypointsDescriptor(resizeReferenceImage);
				std::vector<windage::FeaturePoint>* tempReferenceKeypoints = this->detector->GetKeypoints();

				// add reference feature repository
				double xScaleFactor = this->realWidth / (double)resizeReferenceImage->width;
				double yScaleFactor = this->realHeight / (double)resizeReferenceImage->height;
				for(unsigned int j=0; j<tempReferenceKeypoints->size(); j++)
				{
					windage::Vector3 point = (*tempReferenceKeypoints)[j].GetPoint();
					point.x *= xScaleFactor;
					point.y *= yScaleFactor;
					point.x -= (double)this->realWidth/2.0;
					point.y = (double) this->realHeight/2.0 - point.y + 1;

					(*tempReferenceKeypoints)[j].SetPoint(point);
					(*tempReferenceKeypoints)[j].SetRepositoryID(count);
					(*tempReferenceKeypoints)[j].SetObjectID(j);

					this->referenceRepository[i].push_back((*tempReferenceKeypoints)[j]);
					count++;
				}

				cvReleaseImage(&resizeReferenceImage);
			}
		}

		this->searchTree[i]->Training(&(this->referenceRepository[i]));
	}

	this->trained = true;
	return true;
}

bool MultiplePlanarObjectTracking::UpdateCamerapose(IplImage* grayImage)
{
	if(initialize == false || trained == false)
		return false;

	// featur tracking routine
	{
		std::vector<windage::FeaturePoint> sceneKeypoints1;
		std::vector<windage::FeaturePoint> sceneKeypoints2;

		for(int i=0; i<this->objectCount; i++)
		{
			for(unsigned int j=0; j<this->sceMatchedKeypoints[i].size(); j++)
			{
				sceneKeypoints1.push_back(this->sceMatchedKeypoints[i][j]);
			}
		}

		this->tracker->TrackFeatures(prevImage, grayImage, &sceneKeypoints1, &sceneKeypoints2);
		
		int iter = 0;
		for(int i=0; i<this->objectCount; i++)
		{
			int index = 0;
			for(unsigned int j=0; j<this->sceMatchedKeypoints[i].size(); j++)
			{
				if(sceneKeypoints2[iter].IsOutlier() == false)
				{
					sceMatchedKeypoints[i][index] = sceneKeypoints2[iter];
					index++;
				}
				else 
				{
					this->referenceRepository[i][refMatchedKeypoints[i][index].GetRepositoryID()].SetTracked(false);
					sceMatchedKeypoints[i].erase(sceMatchedKeypoints[i].begin() + index);
					refMatchedKeypoints[i].erase(refMatchedKeypoints[i].begin() + index);
				}
				iter++;
			}
		}
	}

	// feature detection
	{
		this->detector->DoExtractKeypointsDescriptor(grayImage);
		std::vector<windage::FeaturePoint>* sceneKeypoints = this->detector->GetKeypoints();

		for(unsigned int i=0; i<sceneKeypoints->size(); i++)
		{
			int index = this->searchTree[this->step]->Matching((*sceneKeypoints)[i]);
			if(index >= 0)
			{
				// if not tracked have point
				if(this->referenceRepository[this->step][index].IsTracked() == false)
				{
					this->referenceRepository[this->step][index].SetTracked(true);

					refMatchedKeypoints[this->step].push_back(this->referenceRepository[this->step][index]);
					sceMatchedKeypoints[this->step].push_back((*sceneKeypoints)[i]);
				}
			}
		}
	}

	// pose estimate
	for(int i=0; i<this->objectCount; i++)
	{
		if((int)refMatchedKeypoints[i].size() > MIN_FEATURE_POINTS_COUNT)
		{
			this->estimator->AttatchReferencePoint(&(refMatchedKeypoints[i]));
			this->estimator->AttatchScenePoint(&(sceMatchedKeypoints[i]));
			this->estimator->Calculate();
	//		this->estimator->DecomposeHomography((this->cameraParameter[i]));

			// outlier checker
			this->checker->AttatchEstimator(this->estimator);
			this->checker->Calculate();

			// outlier rejection
			for(int j=0; j<(int)refMatchedKeypoints[i].size(); j++)
			{
				if(refMatchedKeypoints[i][j].IsOutlier() == true)
				{
					refMatchedKeypoints[i].erase(refMatchedKeypoints[i].begin() + j);
					sceMatchedKeypoints[i].erase(sceMatchedKeypoints[i].begin() + j);
					j--;
				}
			}

			// refinement
			this->refiner->AttatchHomography(this->estimator->GetHomography());
			this->refiner->AttatchReferencePoint(&(refMatchedKeypoints[i]));
			this->refiner->AttatchScenePoint(&(sceMatchedKeypoints[i]));
			this->refiner->Calculate();

			this->estimator->DecomposeHomography((this->cameraParameter[i]));
		}
	}

	
	for(int i=0; i<this->objectCount; i++)
	{
		
	}

	cvCopyImage(grayImage, this->prevImage);

	this->step++;
	if(this->step >= this->detectionRatio)
		this->step = 0;
	return true;
}

void MultiplePlanarObjectTracking::DrawOutLine(IplImage* colorImage, int objectID, bool drawCross)
{
	int size = 4;
	CvScalar color = CV_RGB(255, 0, 255);
	CvScalar color2 = CV_RGB(255, 255, 255);

	windage::Calibration* calibration = this->cameraParameter[objectID];

	cvLine(colorImage, calibration->ConvertWorld2Image(-this->width/2, -this->height/2, 0.0),	calibration->ConvertWorld2Image(+this->width/2, -this->height/2, 0.0),	color2, 6);
	cvLine(colorImage, calibration->ConvertWorld2Image(+this->width/2, -this->height/2, 0.0),	calibration->ConvertWorld2Image(+this->width/2, +this->height/2, 0.0),	color2, 6);
	cvLine(colorImage, calibration->ConvertWorld2Image(+this->width/2, +this->height/2, 0.0),	calibration->ConvertWorld2Image(-this->width/2, +this->height/2, 0.0),	color2, 6);
	cvLine(colorImage, calibration->ConvertWorld2Image(-this->width/2, +this->height/2, 0.0),	calibration->ConvertWorld2Image(-this->width/2, -this->height/2, 0.0),	color2, 6);

	cvLine(colorImage, calibration->ConvertWorld2Image(-this->width/2, -this->height/2, 0.0),	calibration->ConvertWorld2Image(+this->width/2, -this->height/2, 0.0),	color, 2);
	cvLine(colorImage, calibration->ConvertWorld2Image(+this->width/2, -this->height/2, 0.0),	calibration->ConvertWorld2Image(+this->width/2, +this->height/2, 0.0),	color, 2);
	cvLine(colorImage, calibration->ConvertWorld2Image(+this->width/2, +this->height/2, 0.0),	calibration->ConvertWorld2Image(-this->width/2, +this->height/2, 0.0),	color, 2);
	cvLine(colorImage, calibration->ConvertWorld2Image(-this->width/2, +this->height/2, 0.0),	calibration->ConvertWorld2Image(-this->width/2, -this->height/2, 0.0),	color, 2);

	if(drawCross)
	{
		cvLine(colorImage, calibration->ConvertWorld2Image(-this->width/2, -this->height/2, 0.0),	calibration->ConvertWorld2Image(+this->width/2, +this->height/2, 0.0),	color2, 6);
		cvLine(colorImage, calibration->ConvertWorld2Image(-this->width/2, +this->height/2, 0.0),	calibration->ConvertWorld2Image(+this->width/2, -this->height/2, 0.0),	color2, 6);

		cvLine(colorImage, calibration->ConvertWorld2Image(-this->width/2, -this->height/2, 0.0),	calibration->ConvertWorld2Image(+this->width/2, +this->height/2, 0.0),	color, 2);
		cvLine(colorImage, calibration->ConvertWorld2Image(-this->width/2, +this->height/2, 0.0),	calibration->ConvertWorld2Image(+this->width/2, -this->height/2, 0.0),	color, 2);
	}
}

void MultiplePlanarObjectTracking::DrawDebugInfo(IplImage* colorImage, int objectID)
{
	int pointCount = (int)refMatchedKeypoints.size();
	int r = 255;
	int g = 0;
	int b = 0;

	int size = 4;
	for(unsigned int i=0; i<refMatchedKeypoints[objectID].size(); i++)
	{
		CvPoint referencePoint = cvPoint((int)(refMatchedKeypoints[objectID][i].GetPoint().x * colorImage->width/realWidth + colorImage->width/2),
									(int)(colorImage->height - refMatchedKeypoints[objectID][i].GetPoint().y * colorImage->height/realHeight - colorImage->height/2));
		CvPoint imagePoint = cvPoint((int)sceMatchedKeypoints[objectID][i].GetPoint().x, (int)sceMatchedKeypoints[objectID][i].GetPoint().y);

		cvCircle(colorImage, referencePoint, size, CV_RGB(0, 255, 255), CV_FILLED);
		cvCircle(colorImage, imagePoint, size, CV_RGB(255, 255, 0), CV_FILLED);

		cvLine(colorImage, referencePoint, imagePoint, CV_RGB(255, 0, 0));
	}
}