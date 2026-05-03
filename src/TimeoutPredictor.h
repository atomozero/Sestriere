/*
 * Copyright 2025, Sestriere Authors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * TimeoutPredictor.h — ML-based delivery timeout prediction
 *
 * Simple linear regression on delivery RTT observations.
 * Features: pathLength, messageBytes.
 * Trains on 10+ observations, applies 1.5x safety margin.
 * Compatible with meshcore-open's timeout prediction approach.
 */

#ifndef _TIMEOUT_PREDICTOR_H
#define _TIMEOUT_PREDICTOR_H

#include <SupportDefs.h>

#include <string.h>


static constexpr int32 kMinObservations = 10;
static constexpr int32 kMaxObservations = 100;
static constexpr float kSafetyMargin = 1.5f;
static constexpr bigtime_t kDefaultTimeout = 15000000;  // 15s fallback


struct DeliveryObservation {
	uint8	pathLen;
	uint16	msgBytes;
	uint32	rttMs;
};


class TimeoutPredictor {
public:
	TimeoutPredictor()
		: fCount(0), fTrained(false),
		  fInterceptMs(0), fCoeffPath(0), fCoeffBytes(0)
	{
	}

	void AddObservation(uint8 pathLen, uint16 msgBytes, uint32 rttMs)
	{
		if (rttMs == 0)
			return;

		if (fCount < kMaxObservations) {
			fObs[fCount].pathLen = pathLen;
			fObs[fCount].msgBytes = msgBytes;
			fObs[fCount].rttMs = rttMs;
			fCount++;
		} else {
			// Sliding window: shift left, add at end
			memmove(fObs, fObs + 1,
				(kMaxObservations - 1) * sizeof(DeliveryObservation));
			fObs[kMaxObservations - 1].pathLen = pathLen;
			fObs[kMaxObservations - 1].msgBytes = msgBytes;
			fObs[kMaxObservations - 1].rttMs = rttMs;
		}

		// Retrain periodically
		if (fCount >= kMinObservations && (fCount % 5) == 0)
			_Train();
	}

	// Predict timeout in microseconds
	bigtime_t PredictTimeout(uint8 pathLen, uint16 msgBytes) const
	{
		if (!fTrained)
			return kDefaultTimeout;

		float predicted = fInterceptMs
			+ fCoeffPath * (float)pathLen
			+ fCoeffBytes * (float)msgBytes;

		if (predicted < 3000.0f)
			predicted = 3000.0f;  // Minimum 3 seconds
		if (predicted > 120000.0f)
			predicted = 120000.0f;  // Maximum 2 minutes

		return (bigtime_t)(predicted * kSafetyMargin * 1000.0f);
	}

	bool IsTrained() const { return fTrained; }
	int32 ObservationCount() const { return fCount; }

private:
	void _Train()
	{
		// Simple multivariate linear regression via normal equations
		// y = b0 + b1*x1 + b2*x2
		// Using means and covariances for 2-variable case

		float sumY = 0, sumX1 = 0, sumX2 = 0;
		float sumX1Y = 0, sumX2Y = 0;
		float sumX1X1 = 0, sumX2X2 = 0, sumX1X2 = 0;

		for (int32 i = 0; i < fCount; i++) {
			float y = (float)fObs[i].rttMs;
			float x1 = (float)fObs[i].pathLen;
			float x2 = (float)fObs[i].msgBytes;

			sumY += y;
			sumX1 += x1;
			sumX2 += x2;
			sumX1Y += x1 * y;
			sumX2Y += x2 * y;
			sumX1X1 += x1 * x1;
			sumX2X2 += x2 * x2;
			sumX1X2 += x1 * x2;
		}

		float n = (float)fCount;
		float meanY = sumY / n;
		float meanX1 = sumX1 / n;
		float meanX2 = sumX2 / n;

		// Covariances
		float cov11 = sumX1X1 / n - meanX1 * meanX1;
		float cov22 = sumX2X2 / n - meanX2 * meanX2;
		float cov12 = sumX1X2 / n - meanX1 * meanX2;
		float cov1Y = sumX1Y / n - meanX1 * meanY;
		float cov2Y = sumX2Y / n - meanX2 * meanY;

		// Solve 2x2 system: [cov11 cov12][b1] = [cov1Y]
		//                    [cov12 cov22][b2]   [cov2Y]
		float det = cov11 * cov22 - cov12 * cov12;
		if (det < 1e-6f && det > -1e-6f) {
			// Singular — fall back to simple mean
			fInterceptMs = meanY;
			fCoeffPath = 0;
			fCoeffBytes = 0;
			fTrained = true;
			return;
		}

		fCoeffPath = (cov1Y * cov22 - cov2Y * cov12) / det;
		fCoeffBytes = (cov2Y * cov11 - cov1Y * cov12) / det;
		fInterceptMs = meanY - fCoeffPath * meanX1
			- fCoeffBytes * meanX2;
		fTrained = true;
	}

	DeliveryObservation	fObs[kMaxObservations];
	int32				fCount;
	bool				fTrained;
	float				fInterceptMs;
	float				fCoeffPath;
	float				fCoeffBytes;
};


#endif // _TIMEOUT_PREDICTOR_H
