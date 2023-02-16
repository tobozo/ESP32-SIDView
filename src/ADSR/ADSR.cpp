#include "ADSR.hpp"


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


float ADSR::process()
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


void ADSR::gate(int gate)
{
  if (gate)
    state = env_attack;
  else if (state != env_idle)
    state = env_release;
}


int ADSR::getState()
{
  return state;
}


void ADSR::reset()
{
  state = env_idle;
  output = 0.0;
}


float ADSR::getOutput()
{
  return output;
}


void ADSR::setAttackRate(float rate)
{
  attackRate = rate;
  attackCoef = calcCoef(rate, targetRatioA);
  attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
}


void ADSR::setDecayRate(float rate)
{
  decayRate = rate;
  decayCoef = calcCoef(rate, targetRatioDR);
  decayBase = (sustainLevel - targetRatioDR) * (1.0 - decayCoef);
}


void ADSR::setReleaseRate(float rate)
{
  releaseRate = rate;
  releaseCoef = calcCoef(rate, targetRatioDR);
  releaseBase = -targetRatioDR * (1.0 - releaseCoef);
}


float ADSR::calcCoef(float rate, float targetRatio)
{
  return (rate <= 0) ? 0.0 : exp(-log((1.0 + targetRatio) / targetRatio) / rate);
}


void ADSR::setSustainLevel(float level)
{
  sustainLevel = level;
  decayBase = (sustainLevel - targetRatioDR) * (1.0 - decayCoef);
}


void ADSR::setTargetRatioA(float targetRatio)
{
  if (targetRatio < 0.000000001)
    targetRatio = 0.000000001;  // -180 dB
  targetRatioA = targetRatio;
  attackCoef = calcCoef(attackRate, targetRatioA);
  attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
}


void ADSR::setTargetRatioDR(float targetRatio)
{
  if (targetRatio < 0.000000001)
    targetRatio = 0.000000001;  // -180 dB
  targetRatioDR = targetRatio;
  decayCoef = calcCoef(decayRate, targetRatioDR);
  releaseCoef = calcCoef(releaseRate, targetRatioDR);
  decayBase = (sustainLevel - targetRatioDR) * (1.0 - decayCoef);
  releaseBase = -targetRatioDR * (1.0 - releaseCoef);
}
