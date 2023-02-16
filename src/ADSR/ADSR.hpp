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

#pragma once

#include <math.h>


class ADSR
{
  public:
    ADSR();
    ~ADSR();
    float process();
    float getOutput();
    int getState();
    void gate(int on);
    void setAttackRate(float rate);
    void setDecayRate(float rate);
    void setReleaseRate(float rate);
    void setSustainLevel(float level);
    void setTargetRatioA(float targetRatio);
    void setTargetRatioDR(float targetRatio);
    void reset();

    enum envState
    {
      env_idle = 0,
      env_attack,
      env_decay,
      env_sustain,
      env_release
    };

  protected:
    int state;
    float output;
    float attackRate;
    float decayRate;
    float releaseRate;
    float attackCoef;
    float decayCoef;
    float releaseCoef;
    float sustainLevel;
    float targetRatioA;
    float targetRatioDR;
    float attackBase;
    float decayBase;
    float releaseBase;

    float calcCoef(float rate, float targetRatio);
};

