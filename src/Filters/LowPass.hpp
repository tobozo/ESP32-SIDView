#ifndef LowPass_h
#define LowPass_h

/*
simple resonant filter
inspired from mozzi
*/

class LowPass {

  public:

    LowPass() {  }
    ~LowPass(void) {  }

    void setCutoffFreqAndResonance(float cutoff, float resonance)
    {
      if(cutoff==1.0f) {
        cutoff = 0.999f;
      }
      this->cutoff = cutoff;
      this->resonance = resonance;
      fb = resonance + resonance / ( 1.0f - cutoff );
    }

    float Process(float in) {

      buf0 = buf0 + cutoff * ( in - buf0 + fb * ( buf0 - buf1 ) );
      buf1 = buf1 + cutoff * (buf0 - buf1);
      return buf1;
    }

  protected:

    float cutoff;
    float resonance;
    float fb;
    float buf0;
    float buf1;

};

#endif

