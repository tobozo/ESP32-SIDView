//
//  ADRS.h
//
//  Created by Nigel Redmon on 12/18/12.
//  EarLevel Engineering: earlevel.com
//  Copyright 2012 Nigel Redmon
//
//  For a complete explanation of the ADSR envelope generator and code,
//  read the series of articles by the author, starting here:
//  http://www.earlevel.com/main/2013/06/01/envelope-generators/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code for your own purposes, free or commercial.
//

#ifndef ADRS_h
#define ADRS_h

#include <math.h>


class ADSR
{
  public:
    ADSR();
    ~ADSR();
    double process();
    double getOutput();
    int getState();
    void gate(int on);
    void setAttackRate(double rate);
    void setDecayRate(double rate);
    void setReleaseRate(double rate);
    void setSustainLevel(double level);
    void setTargetRatioA(double targetRatio);
    void setTargetRatioDR(double targetRatio);
    void reset();

    enum envState {
      env_idle = 0,
      env_attack,
      env_decay,
      env_sustain,
      env_release
    };

  protected:
    int state;
    double output;
    double attackRate;
    double decayRate;
    double releaseRate;
    double attackCoef;
    double decayCoef;
    double releaseCoef;
    double sustainLevel;
    double targetRatioA;
    double targetRatioDR;
    double attackBase;
    double decayBase;
    double releaseBase;

    double calcCoef(double rate, double targetRatio);
};

inline double ADSR::process()
{
  switch (state) {
    case env_idle:
      break;
    case env_attack:
      output = attackBase + output * attackCoef;
      if (output >= 1.0) {
        output = 1.0;
        state = env_decay;
      }
      break;
    case env_decay:
      output = decayBase + output * decayCoef;
      if (output <= sustainLevel) {
        output = sustainLevel;
        state = env_sustain;
      }
      break;
    case env_sustain:
      break;
    case env_release:
      output = releaseBase + output * releaseCoef;
      if (output <= 0.0) {
        output = 0.0;
        state = env_idle;
      }
  }
  return output;
}

inline void ADSR::gate(int gate)
{
  if (gate)
    state = env_attack;
  else if (state != env_idle)
    state = env_release;
}

inline int ADSR::getState()
{
  return state;
}

inline void ADSR::reset()
{
  state = env_idle;
  output = 0.0;
}

inline double ADSR::getOutput()
{
  return output;
}



ADSR::ADSR()
{
  reset();
  setAttackRate(0);
  setDecayRate(0);
  setReleaseRate(0);
  setSustainLevel(1.0);
  setTargetRatioA(0.3);
  setTargetRatioDR(0.0001);
}

ADSR::~ADSR()
{
}

void ADSR::setAttackRate(double rate)
{
  attackRate = rate;
  attackCoef = calcCoef(rate, targetRatioA);
  attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
}

void ADSR::setDecayRate(double rate)
{
  decayRate = rate;
  decayCoef = calcCoef(rate, targetRatioDR);
  decayBase = (sustainLevel - targetRatioDR) * (1.0 - decayCoef);
}

void ADSR::setReleaseRate(double rate)
{
  releaseRate = rate;
  releaseCoef = calcCoef(rate, targetRatioDR);
  releaseBase = -targetRatioDR * (1.0 - releaseCoef);
}

double ADSR::calcCoef(double rate, double targetRatio)
{
  return (rate <= 0) ? 0.0 : exp(-log((1.0 + targetRatio) / targetRatio) / rate);
}

void ADSR::setSustainLevel(double level)
{
  sustainLevel = level;
  decayBase = (sustainLevel - targetRatioDR) * (1.0 - decayCoef);
}

void ADSR::setTargetRatioA(double targetRatio)
{
  if (targetRatio < 0.000000001)
    targetRatio = 0.000000001;  // -180 dB
  targetRatioA = targetRatio;
  attackCoef = calcCoef(attackRate, targetRatioA);
  attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
}

void ADSR::setTargetRatioDR(double targetRatio)
{
  if (targetRatio < 0.000000001)
    targetRatio = 0.000000001;  // -180 dB
  targetRatioDR = targetRatio;
  decayCoef = calcCoef(decayRate, targetRatioDR);
  releaseCoef = calcCoef(releaseRate, targetRatioDR);
  decayBase = (sustainLevel - targetRatioDR) * (1.0 - decayCoef);
  releaseBase = -targetRatioDR * (1.0 - releaseCoef);
}

#endif
