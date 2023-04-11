#pragma once

class GameTimer
{
public:
	GameTimer();

	float TotalTime() const; //seconds

	void Reset();
	void Tick();

private:
	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mPrevTime;
	__int64 mBaseTime;
	__int64 mCurrTime;
};