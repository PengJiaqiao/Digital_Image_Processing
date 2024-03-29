//============================================================================
// Name        : Dip2.cpp
// Author      : Ronny Haensch
// Version     : 2.0
// Copyright   : -
// Description :
//============================================================================

#include "Dip2.h"
#include <time.h>

// convolution in spatial domain
/*
src:     input image
kernel:  filter kernel
return:  convolution result
*/
int blockSize; //Block for comparision

Mat Dip2::spatialConvolution(Mat &src, Mat &kernel)
{
	int kSize = kernel.rows;
	int r = (kSize - 1) / 2;
	int rows = src.rows;
	int cols = src.cols;
	Mat dst(rows, cols, CV_32FC1);
	Mat src_padding(rows + kSize - 1, cols + kSize - 1, CV_32FC1);
	Mat kernel_flipped(kernel.rows, kernel.cols, CV_32FC1);
	copyMakeBorder(src, src_padding, r, r, r, r, BORDER_REPLICATE);

	int i, j;
	int _r = 2 * r;
	for (i = 0; i < kernel.rows; i++)
	{
		float *kf_data = kernel_flipped.ptr<float>(i);
		for (j = 0; j < kernel.cols; j++)
		{
			*kf_data++ = kernel.at<float>(_r - i, _r - j); //Coordinates flipped
		}
	}

	int m, n;
	float temp;
	for (i = 0; i < rows; i++)
	{
		float *dst_data = dst.ptr<float>(i);
		for (j = 0; j < cols; j++)
		{
			temp = 0;
			for (m = 0; m < kSize; m++)
			{
				const float *data = src_padding.ptr<float>(i + m);
				data += j;
				const float *kernel_flipped_data = kernel_flipped.ptr<float>(m);
				for (n = 0; n < kSize; n++)
				{
					temp += (*data) * (*kernel_flipped_data);
					data++;
					kernel_flipped_data++;
				}
			}
			*dst_data = temp;
			dst_data++;
		}
	}

	return dst;
}

// the average filter
// HINT: you might want to use Dip2::spatialConvolution(...) within this function
/*
src:     input image
kSize:   window size used by local average
return:  filtered image
*/
Mat Dip2::averageFilter(Mat &src, int kSize)
{
	float weight = (float)1 / (kSize * kSize);
	Mat kernel(kSize, kSize, CV_32FC1, Scalar::all(weight));
	return spatialConvolution(src, kernel);
}

// the median filter
/*
src:     input image
kSize:   window size used by median operation
return:  filtered image
*/
Mat Dip2::medianFilter(Mat &src, int kSize)
{
	int r = kSize / 2;
	int rows = src.rows;
	int cols = src.cols;
	Mat dst(rows, cols, CV_32FC1);
	Mat src_padding(rows + kSize - 1, cols + kSize - 1, CV_32FC1);
	copyMakeBorder(src, src_padding, r, r, r, r, BORDER_REPLICATE);
	int i, j, m, n, count = 0;
	int k_square = kSize * kSize;
	float temp[k_square];

	for (i = 0; i < rows; i++)
	{
		float *dst_data = dst.ptr<float>(i);
		for (j = 0; j < cols; j++)
		{
			for (m = 0; m < kSize; m++)
			{
				const float *data = src_padding.ptr<float>(i + m);
				data += j;
				for (n = 0; n < kSize; n++)
				{
					temp[count++] = *data++;
				}
			}
			std::sort(temp, temp + k_square);
			*dst_data = temp[k_square / 2];
			dst_data++;
			count = 0;
		}
	}
	
	/*
	//Implementation of Faster Median Filter
	//LeetCode 480: Sliding Window Median
	for (i = 0; i < rows; i++)
	{
		float *dst_data = dst.ptr<float>(i);
		for (m = 0; m < kSize; m++)
		{
			const float *data = src_padding.ptr<float>(i + m);
			for (n = 0; n < kSize; n++)
			{
				temp[count++] = *data++;
			}
		}
		std::sort(temp, temp + k_square);
		std::multiset<float> small(temp, temp + k_square / 2 + 1);
		std::multiset<float> large(temp + k_square / 2 + 1, temp + k_square);
		for (j = 0; j < cols - 1; j++)
		{
			for (m = 0; m < kSize; m++)
			{
				if (small.count(src_padding.at<float>(m, j)))
					small.erase(small.find(src_padding.at<float>(m, j)));
				else if (large.count(src_padding.at<float>(m, j)))
					large.erase(large.find(src_padding.at<float>(m, j)));

				if (small.size() <= large.size())
				{
					if (large.empty() || src_padding.at<float>(m, kSize + j) <= *large.begin())
						small.insert(src_padding.at<float>(m, kSize + j));
					else
					{
						small.insert(*large.begin());
						large.erase(large.begin());
						large.insert(src_padding.at<float>(m, kSize + j));
					}
				}
				else
				{
					if (src_padding.at<float>(m, kSize + j) >= *small.rbegin())
						large.insert(src_padding.at<float>(m, kSize + j));
					else
					{
						large.insert(*small.rbegin());
						small.erase(--small.end());
						small.insert(src_padding.at<float>(m, kSize + j));
					}
				}
			}

			*dst_data = (((float)*small.rbegin() + *large.begin()) / 2);
			dst_data++;
		}
	}*/

	return dst;
}

// the bilateral filter
/*
src:     input image
kSize:   size of the kernel --> used to compute std-dev of spatial kernel
sigma:   standard-deviation of the radiometric kernel
return:  filtered image
*/
Mat Dip2::bilateralFilter(Mat &src, int kSize, double sigma)
{
	int r = kSize / 2;
	int rows = src.rows;
	int cols = src.cols;
	double sigma_space = -0.1; //(float)-4.5 / (r * r); //choose sigma = r/3: 3 sigma principle
	double sigma_color = -0.5 / (sigma * sigma);
	Mat dst(rows, cols, CV_32FC1);
	Mat src_padding(rows + kSize - 1, cols + kSize - 1, CV_32FC1);
	copyMakeBorder(src, src_padding, r, r, r, r, BORDER_REPLICATE);

	int i, j;
	Mat spatial_kernel(kSize, kSize, CV_32FC1);
	for (i = -r; i < r + 1; i++)
	{
		float *sk_data = spatial_kernel.ptr<float>(i + r);
		for (j = -r; j < r + 1; j++)
		{
			*sk_data++ = (float)std::exp((i * i + j * j) * sigma_space);
		}
	}

	int m, n, count = 0;
	float exp, temp, Z, weight_max = 0; //Z: Normalizing constant
	for (i = 0; i < rows; i++)
	{
		float *dst_data = dst.ptr<float>(i);
		const float *_data = src.ptr<float>(i);
		for (j = 0; j < cols; j++)
		{

			for (m = 0; m < kSize; m++)
			{
				const float *data = src_padding.ptr<float>(i + m);
				data += j;
				const float *sk_data = spatial_kernel.ptr<float>(m);
				for (n = 0; n < kSize; n++)
				{
					exp = std::exp((*data - *_data) * (*data - *_data) * sigma_color);
					/*if (exp == 1)
						exp = weight_max;
					else if(exp > weight_max)
						weight_max = exp;*/
					Z += (*sk_data) * exp;
					temp += (*data++) * (*sk_data++) * exp;
				}
			}
			*dst_data++ = (float)temp / Z;
			_data++;
			temp = 0;
			Z = 0;
		}
	}

	return dst;
}

// the non-local means filter
/*
src:   		input image
searchSize: size of search region
sigma: 		Optional parameter for weighting function
return:  	filtered image
*/
Mat Dip2::nlmFilter(Mat &src, int searchSize, double sigma)
{
	int blockSize = 5; //Block for comparision
	int rows = src.rows;
	int cols = src.cols;
	int r = searchSize / 2;
	int _r = blockSize / 2;
	double _sigma = -0.5 / (sigma * sigma);
	Mat src_padding(rows + searchSize + blockSize - 2, cols + searchSize + blockSize - 2, CV_32FC1);
	Mat dst(rows, cols, CV_32FC1, Scalar::all(0));
	copyMakeBorder(src, src_padding, r + _r, r + _r, r + _r, r + _r, BORDER_REPLICATE);
	int i, j, m, n, k, l;
	float Z, temp, weight_max; //Z: Normalizing constant; temp: Sum of Euclidean distance from each points in the block to the central block

	for (i = 0; i < rows; i++)
	{
		float *dst_data = dst.ptr<float>(i);
		for (j = 0; j < cols; j++) //i, j used for computing the final output
		{
			Mat weight(searchSize, searchSize, CV_32FC1);
			Z = 0;
			weight_max = 0;
			for (m = 0; m < searchSize; m++) //Compute weight matrix
			{
				float *weight_data = weight.ptr<float>(m);
				for (n = 0; n < searchSize; n++)
				{
					temp = 0;
					for (k = 0; k < blockSize; k++) //k, l used for computing weight by pixels surrounding (m, n) in search zone
					{
						const float *src_data = src_padding.ptr<float>(i + r + k); //Begin from the top-left of central block
						src_data += j + r;
						const float *block_data = src_padding.ptr<float>(i + m + k); //Begin from the top-left of each block in search zone
						block_data += j + n;
						for (l = 0; l < blockSize; l++)
						{
							temp += (*block_data - *src_data) * (*block_data++ - *src_data++);
						}
					}
					*weight_data = std::exp(temp / (blockSize * blockSize) * _sigma);
					/*if (*weight_data != 1 && *weight_data > weight_max)
						weight_max = *weight_data;*/
					Z += *weight_data++;
				}
			}
			/*weight.at<float>(r, r) = weight_max; //The weight of central point in the search zone (i , j)
			Z -= 1 - weight_max;*/

			for (m = 0; m < searchSize; m++) //Compute the final output
			{
				const float *weight_data = weight.ptr<float>(m);
				const float *src_data = src_padding.ptr<float>(i + _r + m);
				src_data += j + _r;
				for (n = 0; n < searchSize; n++)
				{
					*dst_data += (*weight_data++) * (*src_data++);
				}
			}
			*dst_data++ /= Z;
		}
	}

	return dst;
}

/* *****************************
  GIVEN FUNCTIONS
***************************** */

// function loads input image, calls processing function, and saves result
void Dip2::run(void)
{

	// load images as grayscale
	cout << "load images" << endl;
	Mat noise1 = imread("noiseType_1.jpg", 0);
	if (!noise1.data)
	{
		cout << "noiseType_1.jpg not found" << endl;
		cout << "Press enter to exit" << endl;
		cin.get();
		exit(-3);
	}
	noise1.convertTo(noise1, CV_32FC1);
	Mat noise2 = imread("noiseType_2.jpg", 0);
	if (!noise2.data)
	{
		cout << "noiseType_2.jpg not found" << endl;
		cout << "Press enter to exit" << endl;
		cin.get();
		exit(-3);
	}
	noise2.convertTo(noise2, CV_32FC1);
	cout << "done" << endl;

	// apply noise reduction
	// TO DO !!!
	// ==> Choose appropriate noise reduction technique with appropriate parameters
	// ==> "average" or "median"? Why?
	// ==> try also "bilateral" (and if implemented "nlm")
	//clock_t t;
	//t = clock();

	cout << "reduce noise" << endl;
	Mat restorated1 = noiseReduction(noise1, "median", 5);
	Mat restorated2 = noiseReduction(noise2, "nlm", 35, 24);
	cout << "done" << endl;

	// save images
	cout << "save results" << endl;
	imwrite("restorated1.jpg", restorated1);
	imwrite("restorated2.jpg", restorated2);
	cout << "done" << endl;

	//t = clock() - t;
	//printf("It took me %d clicks (%f seconds).\n", t, ((float)t) / CLOCKS_PER_SEC);
}

// noise reduction
/*
src:     input image
method:  name of noise reduction method that shall be performed
	     "average" ==> moving average
         "median" ==> median filter
         "bilateral" ==> bilateral filter
         "nlm" ==> non-local means filter
kSize:   (spatial) kernel size
param:   if method == "bilateral", standard-deviation of radiometric kernel; if method == "nlm", (optional) parameter for similarity function
         can be ignored otherwise (default value = 0)
return:  output image
*/
Mat Dip2::noiseReduction(Mat &src, string method, int kSize, double param)
{

	// apply moving average filter
	if (method.compare("average") == 0)
	{
		return averageFilter(src, kSize);
	}
	// apply median filter
	if (method.compare("median") == 0)
	{
		return medianFilter(src, kSize);
	}
	// apply bilateral filter
	if (method.compare("bilateral") == 0)
	{
		return bilateralFilter(src, kSize, param);
	}
	// apply adaptive average filter
	if (method.compare("nlm") == 0)
	{
		return nlmFilter(src, kSize, param);
	}

	// if none of above, throw warning and return copy of original
	cout << "WARNING: Unknown filtering method! Returning original" << endl;
	cout << "Press enter to continue" << endl;
	cin.get();
	return src.clone();
}

// generates and saves different noisy versions of input image
/*
fname:   path to the input image
*/
void Dip2::generateNoisyImages(string fname)
{

	// load image, force gray-scale
	cout << "load original image" << endl;
	Mat img = imread(fname, 0);
	if (!img.data)
	{
		cout << "ERROR: file " << fname << " not found" << endl;
		cout << "Press enter to exit" << endl;
		cin.get();
		exit(-3);
	}

	// convert to floating point precision
	img.convertTo(img, CV_32FC1);
	cout << "done" << endl;

	// save original
	imwrite("original.jpg", img);

	// generate images with different types of noise
	cout << "generate noisy images" << endl;

	// some temporary images
	Mat tmp1(img.rows, img.cols, CV_32FC1);
	Mat tmp2(img.rows, img.cols, CV_32FC1);
	// first noise operation - Shot Noise
	float noiseLevel = 0.15;
	randu(tmp1, 0, 1);
	threshold(tmp1, tmp2, noiseLevel, 1, CV_THRESH_BINARY);
	multiply(tmp2, img, tmp2);
	threshold(tmp1, tmp1, 1 - noiseLevel, 1, CV_THRESH_BINARY);
	tmp1 *= 255;
	tmp1 = tmp2 + tmp1;
	threshold(tmp1, tmp1, 255, 255, CV_THRESH_TRUNC);
	// save image
	imwrite("noiseType_1.jpg", tmp1);

	// second noise operation - Gaussian Noise
	noiseLevel = 50;
	randn(tmp1, 0, noiseLevel);
	tmp1 = img + tmp1;
	threshold(tmp1, tmp1, 255, 255, CV_THRESH_TRUNC);
	threshold(tmp1, tmp1, 0, 0, CV_THRESH_TOZERO);
	// save image
	imwrite("noiseType_2.jpg", tmp1);

	cout << "done" << endl;
	cout << "Please run now: dip2 restorate" << endl;
}

// function calls some basic testing routines to test individual functions for correctness
void Dip2::test(void)
{

	test_spatialConvolution();
	test_averageFilter();
	test_medianFilter();

	cout << "Press enter to continue" << endl;
	cin.get();
}

// checks basic properties of the convolution result
void Dip2::test_spatialConvolution(void)
{

	Mat input = Mat::ones(9, 9, CV_32FC1);
	input.at<float>(4, 4) = 255;
	Mat kernel = Mat(3, 3, CV_32FC1, 1. / 9.);

	Mat output = spatialConvolution(input, kernel);

	if ((input.cols != output.cols) || (input.rows != output.rows))
	{
		cout << "ERROR: Dip2::spatialConvolution(): input.size != output.size --> Wrong border handling?" << endl;
		return;
	}
	if ((sum(output.row(0) < 0).val[0] > 0) ||
		(sum(output.row(0) > 255).val[0] > 0) ||
		(sum(output.row(8) < 0).val[0] > 0) ||
		(sum(output.row(8) > 255).val[0] > 0) ||
		(sum(output.col(0) < 0).val[0] > 0) ||
		(sum(output.col(0) > 255).val[0] > 0) ||
		(sum(output.col(8) < 0).val[0] > 0) ||
		(sum(output.col(8) > 255).val[0] > 0))
	{
		cout << "ERROR: Dip2::spatialConvolution(): Border of convolution result contains too large/small values --> Wrong border handling?" << endl;
		return;
	}
	else
	{
		if ((sum(output < 0).val[0] > 0) ||
			(sum(output > 255).val[0] > 0))
		{
			cout << "ERROR: Dip2::spatialConvolution(): Convolution result contains too large/small values!" << endl;
			return;
		}
	}
	float ref[9][9] = {{0, 0, 0, 0, 0, 0, 0, 0, 0},
					   {0, 1, 1, 1, 1, 1, 1, 1, 0},
					   {0, 1, 1, 1, 1, 1, 1, 1, 0},
					   {0, 1, 1, (8 + 255) / 9., (8 + 255) / 9., (8 + 255) / 9., 1, 1, 0},
					   {0, 1, 1, (8 + 255) / 9., (8 + 255) / 9., (8 + 255) / 9., 1, 1, 0},
					   {0, 1, 1, (8 + 255) / 9., (8 + 255) / 9., (8 + 255) / 9., 1, 1, 0},
					   {0, 1, 1, 1, 1, 1, 1, 1, 0},
					   {0, 1, 1, 1, 1, 1, 1, 1, 0},
					   {0, 0, 0, 0, 0, 0, 0, 0, 0}};
	for (int y = 1; y < 8; y++)
	{
		for (int x = 1; x < 8; x++)
		{
			if (abs(output.at<float>(y, x) - ref[y][x]) > 0.0001)
			{
				cout << "ERROR: Dip2::spatialConvolution(): Convolution result contains wrong values!" << endl;
				return;
			}
		}
	}
	input.setTo(0);
	input.at<float>(4, 4) = 255;
	kernel.setTo(0);
	kernel.at<float>(0, 0) = -1;
	output = spatialConvolution(input, kernel);
	if (abs(output.at<float>(5, 5) + 255.) < 0.0001)
	{
		cout << "ERROR: Dip2::spatialConvolution(): Is filter kernel \"flipped\" during convolution? (Check lecture/exercise slides)" << endl;
		return;
	}
	if ((abs(output.at<float>(2, 2) + 255.) < 0.0001) || (abs(output.at<float>(4, 4) + 255.) < 0.0001))
	{
		cout << "ERROR: Dip2::spatialConvolution(): Is anchor point of convolution the centre of the filter kernel? (Check lecture/exercise slides)" << endl;
		return;
	}
	cout << "Message: Dip2::spatialConvolution() seems to be correct" << endl;
}

// checks basic properties of the filtering result
void Dip2::test_averageFilter(void)
{

	Mat input = Mat::ones(9, 9, CV_32FC1);
	input.at<float>(4, 4) = 255;

	Mat output = averageFilter(input, 3);

	if ((input.cols != output.cols) || (input.rows != output.rows))
	{
		cout << "ERROR: Dip2::averageFilter(): input.size != output.size --> Wrong border handling?" << endl;
		return;
	}
	if ((sum(output.row(0) < 0).val[0] > 0) ||
		(sum(output.row(0) > 255).val[0] > 0) ||
		(sum(output.row(8) < 0).val[0] > 0) ||
		(sum(output.row(8) > 255).val[0] > 0) ||
		(sum(output.col(0) < 0).val[0] > 0) ||
		(sum(output.col(0) > 255).val[0] > 0) ||
		(sum(output.col(8) < 0).val[0] > 0) ||
		(sum(output.col(8) > 255).val[0] > 0))
	{
		cout << "ERROR: Dip2::averageFilter(): Border of result contains too large/small values --> Wrong border handling?" << endl;
		return;
	}
	else
	{
		if ((sum(output < 0).val[0] > 0) ||
			(sum(output > 255).val[0] > 0))
		{
			cout << "ERROR: Dip2::averageFilter(): Result contains too large/small values!" << endl;
			return;
		}
	}
	float ref[9][9] = {{0, 0, 0, 0, 0, 0, 0, 0, 0},
					   {0, 1, 1, 1, 1, 1, 1, 1, 0},
					   {0, 1, 1, 1, 1, 1, 1, 1, 0},
					   {0, 1, 1, (8 + 255) / 9., (8 + 255) / 9., (8 + 255) / 9., 1, 1, 0},
					   {0, 1, 1, (8 + 255) / 9., (8 + 255) / 9., (8 + 255) / 9., 1, 1, 0},
					   {0, 1, 1, (8 + 255) / 9., (8 + 255) / 9., (8 + 255) / 9., 1, 1, 0},
					   {0, 1, 1, 1, 1, 1, 1, 1, 0},
					   {0, 1, 1, 1, 1, 1, 1, 1, 0},
					   {0, 0, 0, 0, 0, 0, 0, 0, 0}};
	for (int y = 1; y < 8; y++)
	{
		for (int x = 1; x < 8; x++)
		{
			if (abs(output.at<float>(y, x) - ref[y][x]) > 0.0001)
			{
				cout << "ERROR: Dip2::averageFilter(): Result contains wrong values!" << endl;
				return;
			}
		}
	}
	cout << "Message: Dip2::averageFilter() seems to be correct" << endl;
}

// checks basic properties of the filtering result
void Dip2::test_medianFilter(void)
{

	Mat input = Mat::ones(9, 9, CV_32FC1);
	input.at<float>(4, 4) = 255;

	Mat output = medianFilter(input, 3);

	if ((input.cols != output.cols) || (input.rows != output.rows))
	{
		cout << "ERROR: Dip2::medianFilter(): input.size != output.size --> Wrong border handling?" << endl;
		return;
	}
	if ((sum(output.row(0) < 0).val[0] > 0) ||
		(sum(output.row(0) > 255).val[0] > 0) ||
		(sum(output.row(8) < 0).val[0] > 0) ||
		(sum(output.row(8) > 255).val[0] > 0) ||
		(sum(output.col(0) < 0).val[0] > 0) ||
		(sum(output.col(0) > 255).val[0] > 0) ||
		(sum(output.col(8) < 0).val[0] > 0) ||
		(sum(output.col(8) > 255).val[0] > 0))
	{
		cout << "ERROR: Dip2::medianFilter(): Border of result contains too large/small values --> Wrong border handling?" << endl;
		return;
	}
	else
	{
		if ((sum(output < 0).val[0] > 0) ||
			(sum(output > 255).val[0] > 0))
		{
			cout << "ERROR: Dip2::medianFilter(): Result contains too large/small values!" << endl;
			return;
		}
	}
	for (int y = 1; y < 8; y++)
	{
		for (int x = 1; x < 8; x++)
		{
			if (abs(output.at<float>(y, x) - 1.) > 0.0001)
			{
				cout << "ERROR: Dip2::medianFilter(): Result contains wrong values!" << endl;
				return;
			}
		}
	}
	cout << "Message: Dip2::medianFilter() seems to be correct" << endl;
}
