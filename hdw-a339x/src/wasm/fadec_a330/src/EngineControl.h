#pragma once

#include "RegPolynomials.h"
#include "SimVars.h"
#include "Tables.h"
#include "ThrustLimits.h"
#include "common.h"

#include "ini_type_conversion.h"

#define FILENAME_FADEC_CONF_DIRECTORY "\\work\\AircraftStates\\"
#define FILENAME_FADEC_CONF_FILE_EXTENSION ".ini"
#define CONFIGURATION_SECTION_FUEL "FUEL"

#define CONFIGURATION_SECTION_FUEL_CENTER_QUANTITY "FUEL_CENTER_QUANTITY"
#define CONFIGURATION_SECTION_FUEL_LEFT_QUANTITY "FUEL_LEFT_QUANTITY"
#define CONFIGURATION_SECTION_FUEL_RIGHT_QUANTITY "FUEL_RIGHT_QUANTITY"
#define CONFIGURATION_SECTION_FUEL_LEFT_AUX_QUANTITY "FUEL_LEFT_AUX_QUANTITY"
#define CONFIGURATION_SECTION_FUEL_RIGHT_AUX_QUANTITY "FUEL_RIGHT_AUX_QUANTITY"
#define CONFIGURATION_SECTION_FUEL_TRIM_QUANTITY "FUEL_TRIM_QUANTITY"

/* Values in gallons */
struct Configuration {
  double fuelCenter = 0;
  double fuelLeft = 5535.5;
  double fuelRight = fuelLeft;
  double fuelLeftAux = 478.5;
  double fuelRightAux = fuelLeftAux;
  double fuelTrim = 823.0;
};

class EngineControl {
 private:
  SimVars* simVars;
  EngineRatios* ratios;
  Polynomial* poly;
  Timer timerEngine1;
  Timer timerEngine2;
  Timer timerFuel;

  std::string confFilename = FILENAME_FADEC_CONF_DIRECTORY;

  bool simPaused;
  double animationDeltaTime;
  double timer;
  double ambientTemp;
  double ambientPressure;
  double simOnGround;
  double devState;
  double isReady;

  int engine;
  int egtImbalance;
  int ffImbalance;
  int n3Imbalance;
  double engineState;
  double engineStarter;
  double engineIgniter;

  double packs;
  double nai;
  double wai;

  double simCN1;
  double simN1;
  double simN3;
  double thrust;
  double simN3Engine1Pre;
  double simN3Engine2Pre;
  double deltaN3;
  double thermalEnergy1;
  double thermalEnergy2;
  double oilTemperature;
  double oilTemperatureEngine1Pre;
  double oilTemperatureEngine2Pre;
  double oilTemperatureMax;
  double idleN1;
  double idleN3;
  double idleFF;
  double idleEGT;
  double idleOil;
  double mach;
  double pressAltitude;
  double correctedEGT;
  double correctedFuelFlow;
  double cFbwFF;
  double imbalance;
  int engineImbalanced;
  double paramImbalance;

  const double LBS_TO_KGS = 0.4535934;
  const double KGS_TO_LBS = 1 / 0.4535934;
  const double FUEL_THRESHOLD = 661;  // lbs/sec

  bool isFlexActive = false;
  double prevThrustLimitType = 0;
  double prevFlexTemperature = 0;

  const double waitTime = 10;
  const double transitionTime = 30;

  bool isTransitionActive = false;
  double transitionFactor = 0;
  double transitionStartTime = 0;

  /// <summary>
  /// Generate Idle/ Initial Engine Parameters (non-imbalanced)
  /// </summary>
  void generateIdleParameters(double pressAltitude, double mach, double ambientTemp, double ambientPressure) {
    double idleCN1;
    double idleCFF;

    idleCN1 = iCN1(pressAltitude, mach, ambientTemp);
    idleN1 = idleCN1 * sqrt(ratios->theta2(0, ambientTemp));
    idleN3 = iCN3(pressAltitude, mach) * sqrt(ratios->theta(ambientTemp));
    idleCFF = poly->correctedFuelFlow(idleCN1, 0, pressAltitude);                                               // lbs/hr
    idleFF = idleCFF * LBS_TO_KGS * ratios->delta2(0, ambientPressure) * sqrt(ratios->theta2(0, ambientTemp));  // Kg/hr
    idleEGT = poly->correctedEGT(idleCN1, idleCFF, 0, pressAltitude) * ratios->theta2(0, ambientTemp);

    simVars->setEngineIdleN1(idleN1);
    simVars->setEngineIdleN3(idleN3);
    simVars->setEngineIdleFF(idleFF);
    simVars->setEngineIdleEGT(idleEGT);
  }

  double initOil(int minOil, int maxOil) {
    double idleOil = (rand() % (maxOil - minOil + 1) + minOil) / 10;
    return idleOil;
  }

  /// <summary>
  /// Engine imbalance Coded Digital Word:
  /// 0 - Engine, 00 - EGT, 00 - FuelFlow, 00 - N2, 00 - Oil Qty, 00 - Oil PSI, 00 - Oil PSI Rnd, 00 - Oil Max Temp
  /// Generates a random engine imbalance. Next steps: make realistic imbalance due to wear
  /// </summary>
  void generateEngineImbalance(int initial) {
    int oilQtyImbalance;
    int oilPressureImbalance;
    int oilPressureIdle;
    int oilTemperatureMax;
    std::string imbalanceCode;

    if (initial == 1) {
      // Decide Engine with imbalance
      if ((rand() % 100) + 1 < 50) {
        engine = 1;
      } else {
        engine = 2;
      }
      // Obtain EGT imbalance (Max 20 degree C)
      egtImbalance = (rand() % 20) + 1;

      // Obtain FF imbalance (Max 36 Kg/h)
      ffImbalance = (rand() % 36) + 1;

      // Obtain N3 imbalance (Max 0.3%)
      n3Imbalance = (rand() % 30) + 1;

      // Obtain Oil Qty imbalance (Max 2.0 qt)
      oilQtyImbalance = (rand() % 20) + 1;

      // Obtain Oil Pressure imbalance (Max 3.0 PSI)
      oilPressureImbalance = (rand() % 30) + 1;

      // Obtain Oil Pressure Random Idle (-6 to +6 PSI)
      oilPressureIdle = (rand() % 12) + 1;

      // Obtain Oil Temperature (85 to 95 Celsius)
      oilTemperatureMax = (rand() % 10) + 86;

      // Zero Padding and Merging
      imbalanceCode = to_string_with_zero_padding(engine, 2) + to_string_with_zero_padding(egtImbalance, 2) +
                      to_string_with_zero_padding(ffImbalance, 2) + to_string_with_zero_padding(n3Imbalance, 2) +
                      to_string_with_zero_padding(oilQtyImbalance, 2) + to_string_with_zero_padding(oilPressureImbalance, 2) +
                      to_string_with_zero_padding(oilPressureIdle, 2) + to_string_with_zero_padding(oilTemperatureMax, 2);

      simVars->setEngineImbalance(stod(imbalanceCode));
    }
  }

  /// <summary>
  /// Engine State Machine
  /// 0 - Engine OFF, 1 - Engine ON, 2 - Engine Starting, 3 - Engine Re-starting & 4 - Engine Shutting
  /// </summary>
  void engineStateMachine(int engine,
                          double engineIgniter,
                          double engineStarter,
                          double simN3,
                          double idleN3,
                          double pressAltitude,
                          double ambientTemp,
                          double deltaTimeDiff) {
    int resetTimer = 0;
    double egtFbw = 0;

    switch (engine) {
      case 1:
        engineState = simVars->getEngine1State();
        egtFbw = simVars->getEngine1EGT();
        break;
      case 2:
        engineState = simVars->getEngine2State();
        egtFbw = simVars->getEngine2EGT();
        break;
    }
    // Present State PAUSED
    if (deltaTimeDiff == 0 && engineState < 10) {
      engineState = engineState + 10;
      simPaused = true;
    } else if (deltaTimeDiff == 0 && engineState >= 10) {
      simPaused = true;
    } else {
      simPaused = false;

      // Present State OFF
      if (engineState == 0 || engineState == 10) {
        if (engineIgniter == 1 && engineStarter == 1 && simN3 > 20) {
          engineState = 1;
        } else if (engineIgniter == 2 && engineStarter == 1) {
          engineState = 2;
        } else {
          engineState = 0;
        }
      }

      // Present State ON
      if (engineState == 1 || engineState == 11) {
        if (engineStarter == 1) {
          engineState = 1;
        } else {
          engineState = 4;
        }
      }

      // Present State Starting.
      if (engineState == 2 || engineState == 12) {
        if (engineStarter == 1 && simN3 >= (idleN3 - 0.1)) {
          engineState = 1;
          resetTimer = 1;
        } else if (engineStarter == 0) {
          engineState = 4;
          resetTimer = 1;
        } else {
          engineState = 2;
        }
      }

      // Present State Re-Starting.
      if (engineState == 3 || engineState == 13) {
        if (engineStarter == 1 && simN3 >= (idleN3 - 0.1)) {
          engineState = 1;
          resetTimer = 1;
        } else if (engineStarter == 0) {
          engineState = 4;
          resetTimer = 1;
        } else {
          engineState = 3;
        }
      }

      // Present State Shutting
      if (engineState == 4 || engineState == 14) {
        if (engineIgniter == 2 && engineStarter == 1) {
          engineState = 3;
          resetTimer = 1;
        } else if (engineStarter == 0 && simN3 < 0.05 && egtFbw <= ambientTemp) {
          engineState = 0;
          resetTimer = 1;
        } else if (engineStarter == 1 && simN3 > 50) {
          engineState = 3;
          resetTimer = 1;
        } else {
          engineState = 4;
        }
      }
    }

    switch (engine) {
      case 1:
        simVars->setEngine1State(engineState);
        if (resetTimer == 1) {
          simVars->setEngine1Timer(0);
        }
        break;
      case 2:
        simVars->setEngine2State(engineState);
        if (resetTimer == 1) {
          simVars->setEngine2Timer(0);
        }
        break;
    }
  }

  /// <summary>
  /// Engine Start Procedure
  /// </summary>TIT
  void engineStartProcedure(int engine,
                            double engineState,
                            double imbalance,
                            double deltaTime,
                            double timer,
                            double simN3,
                            double pressAltitude,
                            double ambientTemp) {
    double startCN3Engine1;
    double startCN3Engine2;
    double preN3Fbw;
    double newN3Fbw;
    double preEgtFbw;
    double startEgtFbw;
    double shutdownEgtFbw;

    n3Imbalance = 0;
    ffImbalance = 0;
    egtImbalance = 0;
    idleN3 = simVars->getEngineIdleN3();
    idleN1 = simVars->getEngineIdleN1();
    idleFF = simVars->getEngineIdleFF();
    idleEGT = simVars->getEngineIdleEGT();

    // Engine imbalance
    engineImbalanced = imbalanceExtractor(imbalance, 1);

    // Checking engine imbalance
    if (engineImbalanced == engine) {
      ffImbalance = imbalanceExtractor(imbalance, 3);
      egtImbalance = imbalanceExtractor(imbalance, 2);
      n3Imbalance = imbalanceExtractor(imbalance, 4) / 100;
    }

    if (engine == 1) {
      // Delay between Engine Master ON and Start Valve Open
      if (timer < 1.7) {
        if (simOnGround == 1) {
          simVars->setFuelUsedEngine1(0);
        }
        simVars->setEngine1Timer(timer + deltaTime);
        startCN3Engine1 = 0;
        SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::StartCN3Engine1, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double),
                                      &startCN3Engine1);
      } else {
        preN3Fbw = simVars->getEngine1N3();
        preEgtFbw = simVars->getEngine1EGT();
        newN3Fbw = poly->startN3(simN3, preN3Fbw, idleN3);
        startEgtFbw = poly->startEGT(newN3Fbw, idleN3, ambientTemp, idleEGT);
        shutdownEgtFbw = poly->shutdownEGT(preEgtFbw, ambientTemp, deltaTime);

        simVars->setEngine1N3(newN3Fbw);
        simVars->setEngine1N2(newN3Fbw + 0.7);
        simVars->setEngine1N1(poly->startN1(newN3Fbw, idleN3, idleN1));
        simVars->setEngine1FF(poly->startFF(newN3Fbw, idleN3, idleFF));

        if (engineState == 3) {
          if (abs(startEgtFbw - preEgtFbw) <= 1.5) {
            simVars->setEngine1EGT(startEgtFbw);
            simVars->setEngine1State(2);
          } else if (startEgtFbw > preEgtFbw) {
            simVars->setEngine1EGT(preEgtFbw + (0.75 * deltaTime * (idleN3 - newN3Fbw)));
          } else {
            simVars->setEngine1EGT(shutdownEgtFbw);
          }
        } else {
          simVars->setEngine1EGT(startEgtFbw);
        }

        oilTemperature = poly->startOilTemp(newN3Fbw, idleN3, ambientTemp);
        oilTemperatureEngine1Pre = oilTemperature;
        SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::OilTempEngine1, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double),
                                      &oilTemperature);
      }
    } else {
      if (timer < 1.7) {
        if (simOnGround == 1) {
          simVars->setFuelUsedEngine2(0);
        }
        simVars->setEngine2Timer(timer + deltaTime);
        startCN3Engine2 = 0;
        SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::StartCN3Engine2, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double),
                                      &startCN3Engine2);
      } else {
        preN3Fbw = simVars->getEngine2N3();
        preEgtFbw = simVars->getEngine2EGT();
        newN3Fbw = poly->startN3(simN3, preN3Fbw, idleN3);
        startEgtFbw = poly->startEGT(newN3Fbw, idleN3, ambientTemp, idleEGT);
        shutdownEgtFbw = poly->shutdownEGT(preEgtFbw, ambientTemp, deltaTime);

        simVars->setEngine2N3(newN3Fbw);
        simVars->setEngine2N2(newN3Fbw + 0.7);
        simVars->setEngine2N1(poly->startN1(newN3Fbw, idleN3, idleN1));
        simVars->setEngine2FF(poly->startFF(newN3Fbw, idleN3, idleFF));

        if (engineState == 3) {
          if (abs(startEgtFbw - preEgtFbw) <= 1.5) {
            simVars->setEngine2EGT(startEgtFbw);
            simVars->setEngine2State(2);
          } else if (startEgtFbw > preEgtFbw) {
            simVars->setEngine2EGT(preEgtFbw + (0.75 * deltaTime * (idleN3 - newN3Fbw)));
          } else {
            simVars->setEngine2EGT(shutdownEgtFbw);
          }
        } else {
          simVars->setEngine2EGT(startEgtFbw);
        }

        oilTemperature = poly->startOilTemp(newN3Fbw, idleN3, ambientTemp);
        oilTemperatureEngine2Pre = oilTemperature;
        SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::OilTempEngine2, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double),
                                      &oilTemperature);
      }
    }
  }

  /// <summary>
  /// Engine Shutdown Procedure - TEMPORAL SOLUTION
  /// </summary>
  void engineShutdownProcedure(int engine, double ambientTemp, double simN1, double deltaTime, double timer) {
    double preN1Fbw;
    double preN3Fbw;
    double preEgtFbw;
    double newN1Fbw;
    double newN3Fbw;
    double newEgtFbw;

    if (engine == 1) {
      if (timer < 1.8) {
        simVars->setEngine1Timer(timer + deltaTime);
      } else {
        preN1Fbw = simVars->getEngine1N1();
        preN3Fbw = simVars->getEngine1N3();
        preEgtFbw = simVars->getEngine1EGT();
        newN1Fbw = poly->shutdownN1(preN1Fbw, deltaTime);
        if (simN1 < 5 && simN1 > newN1Fbw) {  // Takes care of windmilling
          newN1Fbw = simN1;
        }
        newN3Fbw = poly->shutdownN3(preN3Fbw, deltaTime);
        newEgtFbw = poly->shutdownEGT(preEgtFbw, ambientTemp, deltaTime);
        simVars->setEngine1N1(newN1Fbw);
        simVars->setEngine1N2(newN3Fbw + 0.7);
        simVars->setEngine1N3(newN3Fbw);
        simVars->setEngine1EGT(newEgtFbw);
      }
    } else if (engine == 2) {
      if (timer < 1.8) {
        simVars->setEngine2Timer(timer + deltaTime);
      } else {
        preN1Fbw = simVars->getEngine2N1();
        preN3Fbw = simVars->getEngine2N3();
        preEgtFbw = simVars->getEngine2EGT();
        newN1Fbw = poly->shutdownN1(preN1Fbw, deltaTime);
        if (simN1 < 5 && simN1 > newN1Fbw) {  // Takes care of windmilling
          newN1Fbw = simN1;
        }
        newN3Fbw = poly->shutdownN3(preN3Fbw, deltaTime);
        newEgtFbw = poly->shutdownEGT(preEgtFbw, ambientTemp, deltaTime);
        simVars->setEngine2N1(newN1Fbw);
        simVars->setEngine2N2(newN3Fbw + 0.7);
        simVars->setEngine2N3(newN3Fbw);
        simVars->setEngine2EGT(newEgtFbw);
      }
    }
  }
  /// <summary>
  /// FBW Engine RPM (N1, N2 and N3)
  /// Updates Engine N1, N2 and N3 with our own algorithm for start-up and shutdown
  /// </summary>
  void updatePrimaryParameters(int engine, double imbalance, double simN1, double simN2) {
    // Engine imbalance
    engineImbalanced = imbalanceExtractor(imbalance, 1);
    paramImbalance = imbalanceExtractor(imbalance, 4) / 100;

    // Checking engine imbalance
    if (engineImbalanced != engine) {
      paramImbalance = 0;
    }

    if (engine == 1) {
      simVars->setEngine1N1(simN1);
      simVars->setEngine1N2(max(0, simN2 - paramImbalance));
      simVars->setEngine1N3(simN3);
    } else {
      simVars->setEngine2N1(simN1);
      simVars->setEngine2N2(max(0, simN2 - paramImbalance));
      simVars->setEngine2N3(simN3);
    }
  }

  /// <summary>
  /// FBW Exhaust Gas Temperature (in degree Celsius)
  /// Updates EGT with realistic values visualized in the ECAM
  /// </summary>
  void updateEGT(int engine,
                 double imbalance,
                 double deltaTime,
                 double simOnGround,
                 double engineState,
                 double simCN1,
                 double cFbwFF,
                 double mach,
                 double pressAltitude,
                 double ambientTemp) {
    double egtFbwPreviousEng1;
    double egtFbwActualEng1;
    double egtFbwPreviousEng2;
    double egtFbwActualEng2;

    // Engine imbalance timer
    engineImbalanced = imbalanceExtractor(imbalance, 1);
    paramImbalance = imbalanceExtractor(imbalance, 2);

    correctedEGT = poly->correctedEGT(simCN1, cFbwFF, mach, pressAltitude);

    // Checking engine imbalance
    if (engineImbalanced != engine) {
      paramImbalance = 0;
    }

    if (engine == 1) {
      if (simOnGround == 1 && engineState == 0) {
        simVars->setEngine1EGT(ambientTemp);
      } else {
        egtFbwPreviousEng1 = simVars->getEngine1EGT();
        egtFbwActualEng1 = (correctedEGT * ratios->theta2(mach, ambientTemp)) - paramImbalance;
        egtFbwActualEng1 = egtFbwActualEng1 + (egtFbwPreviousEng1 - egtFbwActualEng1) * expFBW(-0.1 * deltaTime);
        simVars->setEngine1EGT(egtFbwActualEng1);
      }
    } else {
      if (simOnGround == 1 && engineState == 0) {
        simVars->setEngine2EGT(ambientTemp);
      } else {
        egtFbwPreviousEng2 = simVars->getEngine2EGT();
        egtFbwActualEng2 = (correctedEGT * ratios->theta2(mach, ambientTemp)) - paramImbalance;
        egtFbwActualEng2 = egtFbwActualEng2 + (egtFbwPreviousEng2 - egtFbwActualEng2) * expFBW(-0.1 * deltaTime);
        simVars->setEngine2EGT(egtFbwActualEng2);
      }
    }
  }

  /// <summary>
  /// FBW Fuel FLow (in Kg/h)
  /// Updates Fuel Flow with realistic values
  /// </summary>
  double
  updateFF(int engine, double imbalance, double simCN1, double mach, double pressAltitude, double ambientTemp, double ambientPressure) {
    double outFlow = 0;

    // Engine imbalance
    engineImbalanced = imbalanceExtractor(imbalance, 1);
    paramImbalance = imbalanceExtractor(imbalance, 3);

    correctedFuelFlow = poly->correctedFuelFlow(simCN1, mach, pressAltitude);  // in lbs/hr.

    // Checking engine imbalance
    if (engineImbalanced != engine || correctedFuelFlow < 1) {
      paramImbalance = 0;
    }

    // Checking Fuel Logic and final Fuel Flow
    if (correctedFuelFlow < 1) {
      outFlow = 0;
    } else {
      outFlow = max(0, (correctedFuelFlow * LBS_TO_KGS * ratios->delta2(mach, ambientPressure) * sqrt(ratios->theta2(mach, ambientTemp))) -
                           paramImbalance);
    }

    if (engine == 1) {
      simVars->setEngine1FF(outFlow);
    } else {
      simVars->setEngine2FF(outFlow);
    }

    return correctedFuelFlow;
  }

  /// <summary>
  /// FBW Oil Qty, Pressure and Temperature (in Quarts, PSI and degree Celsius)
  /// Updates Oil with realistic values visualized in the SD
  /// </summary>
  void updateOil(int engine, double thrust, double simN3, double deltaN3, double deltaTime, double ambientTemp) {
    double steadyTemperature;
    double thermalEnergy;
    double oilTemperaturePre;
    double oilQtyActual;
    double oilTotalActual;
    double oilQtyObjective;
    double oilBurn;
    double oilIdleRandom;
    double oilPressure;

    //--------------------------------------------
    // Engine Reading
    //--------------------------------------------
    if (engine == 1) {
      steadyTemperature = simVars->getEngine1EGT();
      thermalEnergy = thermalEnergy1;
      oilTemperaturePre = oilTemperatureEngine1Pre;
      oilQtyActual = simVars->getEngine1Oil();
      oilTotalActual = simVars->getEngine1TotalOil();
    } else {
      steadyTemperature = simVars->getEngine2EGT();
      thermalEnergy = thermalEnergy2;
      oilTemperaturePre = oilTemperatureEngine2Pre;
      oilQtyActual = simVars->getEngine2Oil();
      oilTotalActual = simVars->getEngine2TotalOil();
    }

    //--------------------------------------------
    // Oil Temperature
    //--------------------------------------------
    if (simOnGround == 1 && engineState == 0 && ambientTemp > oilTemperaturePre - 10) {
      oilTemperature = ambientTemp;
    } else {
      if (steadyTemperature > oilTemperatureMax) {
        steadyTemperature = oilTemperatureMax;
      }
      thermalEnergy = (0.995 * thermalEnergy) + (deltaN3 / deltaTime);
      oilTemperature = poly->oilTemperature(thermalEnergy, oilTemperaturePre, steadyTemperature, deltaTime);
    }

    //--------------------------------------------
    // Oil Quantity
    //--------------------------------------------
    // Calculating Oil Qty as a function of thrust
    oilQtyObjective = oilTotalActual * (1 - poly->oilGulpPct(thrust));
    oilQtyActual = oilQtyActual - (oilTemperature - oilTemperaturePre);

    // Oil burnt taken into account for tank and total oil
    oilBurn = (0.00011111 * deltaTime);
    oilQtyActual = oilQtyActual - oilBurn;
    oilTotalActual = oilTotalActual - oilBurn;

    //--------------------------------------------
    // Oil Pressure
    //--------------------------------------------
    // Engine imbalance
    engineImbalanced = imbalanceExtractor(imbalance, 1);
    paramImbalance = imbalanceExtractor(imbalance, 6) / 10;
    oilIdleRandom = imbalanceExtractor(imbalance, 7) - 6;

    // Checking engine imbalance
    if (engineImbalanced != engine) {
      paramImbalance = 0;
    }

    oilPressure = poly->oilPressure(simN3) - paramImbalance + oilIdleRandom;

    //--------------------------------------------
    // Engine Writing
    //--------------------------------------------
    if (engine == 1) {
      thermalEnergy1 = thermalEnergy;
      oilTemperatureEngine1Pre = oilTemperature;
      simVars->setEngine1Oil(oilQtyActual);
      simVars->setEngine1TotalOil(oilTotalActual);
      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::OilTempEngine1, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double),
                                    &oilTemperature);
      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::OilPsiEngine1, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double), &oilPressure);
    } else {
      thermalEnergy2 = thermalEnergy;
      oilTemperatureEngine2Pre = oilTemperature;
      simVars->setEngine2Oil(oilQtyActual);
      simVars->setEngine2TotalOil(oilTotalActual);
      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::OilTempEngine2, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double),
                                    &oilTemperature);
      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::OilPsiEngine2, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double), &oilPressure);
    }
  }

  /// @brief FBW Fuel Consumption and Tankering
  /// Updates Fuel Consumption with realistic values
  /// @param deltaTimeSeconds Frame delta time in seconds
  void updateFuel(double deltaTimeSeconds) {
    double m = 0;
    double b = 0;
    double fuelBurn1 = 0;
    double fuelBurn2 = 0;
    double apuBurn1 = 0;
    double apuBurn2 = 0;

    double refuelRate = simVars->getRefuelRate();
    double refuelStartedByUser = simVars->getRefuelStartedByUser();
    bool uiFuelTamper = false;

    double pumpStateEngine1 = simVars->getPumpStateEngine1();
    double pumpStateEngine2 = simVars->getPumpStateEngine2();
    bool xfrCenterLeftManual = simVars->getJunctionSetting(4) > 1.5;
    bool xfrCenterRightManual = simVars->getJunctionSetting(5) > 1.5;
    bool xfrCenterLeftAuto = simVars->getValve(11) > 0.0 && !xfrCenterLeftManual;
    bool xfrCenterRightAuto = simVars->getValve(12) > 0.0 && !xfrCenterRightManual;
    bool xfrValveCenterLeftOpen = simVars->getValve(9) > 0.0 && (xfrCenterLeftAuto || xfrCenterLeftManual);
    bool xfrValveCenterRightOpen = simVars->getValve(10) > 0.0 && (xfrCenterRightAuto || xfrCenterRightManual);

    double xfrValveOuterLeft1 = simVars->getValve(6);
    double xfrValveOuterLeft2 = simVars->getValve(4);
    double xfrValveOuterRight1 = simVars->getValve(7);
    double xfrValveOuterRight2 = simVars->getValve(5);
    double lineLeftToCenterFlow = simVars->getLineFlow(27);
    double lineRightToCenterFlow = simVars->getLineFlow(28);
    double lineFlowRatio = 0;

    double engine1PreFF = simVars->getEngine1PreFF();  // KG/H
    double engine2PreFF = simVars->getEngine2PreFF();  // KG/H
    double engine1FF = simVars->getEngine1FF();        // KG/H
    double engine2FF = simVars->getEngine2FF();        // KG/H

    /// weight of one gallon of fuel in pounds
    double fuelWeightGallon = simVars->getFuelWeightGallon();
    double fuelUsedEngine1 = simVars->getFuelUsedEngine1();  // Kg
    double fuelUsedEngine2 = simVars->getFuelUsedEngine2();  // Kg

    double fuelLeftPre = simVars->getFuelLeftPre();                                // LBS
    double fuelRightPre = simVars->getFuelRightPre();                              // LBS
    double fuelAuxLeftPre = simVars->getFuelAuxLeftPre();                          // LBS
    double fuelAuxRightPre = simVars->getFuelAuxRightPre();                        // LBS
    double fuelCenterPre = simVars->getFuelCenterPre();                            // LBS
    double fuelTrimPre = simVars->getFuelTrimPre();                                // LBS
    double leftQuantity = simVars->getFuelTankQuantity(2) * fuelWeightGallon;      // LBS
    double rightQuantity = simVars->getFuelTankQuantity(3) * fuelWeightGallon;     // LBS
    double leftAuxQuantity = simVars->getFuelTankQuantity(4) * fuelWeightGallon;   // LBS
    double rightAuxQuantity = simVars->getFuelTankQuantity(5) * fuelWeightGallon;  // LBS
    double centerQuantity = simVars->getFuelTankQuantity(1) * fuelWeightGallon;    // LBS
    double trimQuantity = simVars->getFuelTankQuantity(6) * fuelWeightGallon;      // LBS

    double fuelLeft = 0;
    double fuelRight = 0;
    double fuelLeftAux = 0;
    double fuelRightAux = 0;
    double fuelCenter = 0;
    double fuelTrim = 0;
    double xfrCenterToLeft = 0;
    double xfrCenterToRight = 0;
    double xfrAuxLeft = 0;
    double xfrAuxRight = 0;
    double fuelTotalActual = leftQuantity + rightQuantity + leftAuxQuantity + rightAuxQuantity + centerQuantity;  // + trimQuantity;  // LBS
    double fuelTotalPre = fuelLeftPre + fuelRightPre + fuelAuxLeftPre + fuelAuxRightPre + fuelCenterPre;  // + fuelTrimPre;           // LBS
    double deltaFuelRate = abs(fuelTotalActual - fuelTotalPre) / (fuelWeightGallon * deltaTimeSeconds);   // LBS/ sec

    double engine1State = simVars->getEngine1State();
    double engine2State = simVars->getEngine2State();

    int isTankClosed = 0;
    double xFeedValve = simVars->getValve(3);
    double leftPump1 = simVars->getPump(2);
    double leftPump2 = simVars->getPump(5);
    double rightPump1 = simVars->getPump(3);
    double rightPump2 = simVars->getPump(6);

    // Check Ready & Development State for UI
    isReady = simVars->getIsReady();
    devState = simVars->getDeveloperState();

    /// Delta time for this update in hours
    double deltaTime = deltaTimeSeconds / 3600;

    // Pump State Logic for Left Wing
    if (pumpStateEngine1 == 0 && (timerEngine1.elapsed() == 0 || timerEngine1.elapsed() >= 1000)) {
      if (fuelLeftPre - leftQuantity > 0 && leftQuantity == 0) {
        timerEngine1.reset();
        simVars->setPumpStateEngine1(1);
      } else if (fuelLeftPre == 0 && leftQuantity - fuelLeftPre > 0) {
        timerEngine1.reset();
        simVars->setPumpStateEngine1(2);
      } else {
        simVars->setPumpStateEngine1(0);
      }
    } else if (pumpStateEngine1 == 1 && timerEngine1.elapsed() >= 2100) {
      simVars->setPumpStateEngine1(0);
      timerEngine1.reset();
    } else if (pumpStateEngine1 == 2 && timerEngine1.elapsed() >= 2700) {
      simVars->setPumpStateEngine1(0);
      timerEngine1.reset();
    }

    // Pump State Logic for Right Wing
    if (pumpStateEngine2 == 0 && (timerEngine2.elapsed() == 0 || timerEngine2.elapsed() >= 1000)) {
      if (fuelRightPre - rightQuantity > 0 && rightQuantity == 0) {
        timerEngine2.reset();
        simVars->setPumpStateEngine2(1);
      } else if (fuelRightPre == 0 && rightQuantity - fuelRightPre > 0) {
        timerEngine2.reset();
        simVars->setPumpStateEngine2(2);
      } else {
        simVars->setPumpStateEngine2(0);
      }
    } else if (pumpStateEngine2 == 1 && timerEngine2.elapsed() >= 2100) {
      simVars->setPumpStateEngine2(0);
      timerEngine2.reset();
    } else if (pumpStateEngine2 == 2 && timerEngine2.elapsed() >= 2700) {
      simVars->setPumpStateEngine2(0);
      timerEngine2.reset();
    }

    // Checking for in-game UI Fuel tampering
    if ((isReady == 1 && refuelStartedByUser == 0 && deltaFuelRate > FUEL_THRESHOLD) ||
        (isReady == 1 && refuelStartedByUser == 1 && deltaFuelRate > FUEL_THRESHOLD && refuelRate < 2)) {
      uiFuelTamper = true;
    }

    if (simPaused || uiFuelTamper && devState == 0) {  // Detects whether the Sim is paused or the Fuel UI is being tampered with
      simVars->setFuelLeftPre(fuelLeftPre);            // in LBS
      simVars->setFuelRightPre(fuelRightPre);          // in LBS
      simVars->setFuelAuxLeftPre(fuelAuxLeftPre);      // in LBS
      simVars->setFuelAuxRightPre(fuelAuxRightPre);    // in LBS
      simVars->setFuelCenterPre(fuelCenterPre);        // in LBS
      simVars->setFuelTrimPre(fuelTrimPre);            // in LBS

      fuelLeft = (fuelLeftPre / fuelWeightGallon);          // USG
      fuelRight = (fuelRightPre / fuelWeightGallon);        // USG
      fuelCenter = (fuelCenterPre / fuelWeightGallon);      // USG
      fuelLeftAux = (fuelAuxLeftPre / fuelWeightGallon);    // USG
      fuelRightAux = (fuelAuxRightPre / fuelWeightGallon);  // USG
      fuelTrim = (fuelTrimPre / fuelWeightGallon);          // USG

      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::FuelCenterMain, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double), &fuelCenter);
      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::FuelLeftMain, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double), &fuelLeft);
      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::FuelRightMain, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double), &fuelRight);
      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::FuelLeftAux, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double), &fuelLeftAux);
      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::FuelRightAux, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double), &fuelRightAux);
      // SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::FuelSystemTrim, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double),
      // &fuelTrim);
    } else if (!uiFuelTamper && refuelStartedByUser == 1) {  // Detects refueling from the EFB
      simVars->setFuelLeftPre(leftQuantity);                 // in LBS
      simVars->setFuelRightPre(rightQuantity);               // in LBS
      simVars->setFuelAuxLeftPre(leftAuxQuantity);           // in LBS
      simVars->setFuelAuxRightPre(rightAuxQuantity);         // in LBS
      simVars->setFuelCenterPre(centerQuantity);             // in LBS
      simVars->setFuelTrimPre(trimQuantity);                 // in LBS
    } else {
      if (uiFuelTamper == 1) {
        fuelLeftPre = leftQuantity;          // LBS
        fuelRightPre = rightQuantity;        // LBS
        fuelAuxLeftPre = leftAuxQuantity;    // LBS
        fuelAuxRightPre = rightAuxQuantity;  // LBS
        fuelCenterPre = centerQuantity;      // LBS
        fuelTrimPre = trimQuantity;          // LBS
      }
      //-----------------------------------------------------------
      // Cross-feed Logic
      // isTankClosed = 0, x-feed valve closed
      // isTankClosed = 1, left tank does not supply fuel
      // isTankClosed = 2, right tank does not supply fuel
      // isTankClosed = 3, left & right tanks do not supply fuel
      // isTankClosed = 4, both tanks supply fuel
      if (xFeedValve > 0.0) {
        if (leftPump1 == 0 && leftPump2 == 0 && rightPump1 == 0 && rightPump2 == 0)
          isTankClosed = 3;
        else if (leftPump1 == 0 && leftPump2 == 0)
          isTankClosed = 1;
        else if (rightPump1 == 0 && rightPump2 == 0)
          isTankClosed = 2;
        else
          isTankClosed = 4;
      }

      //--------------------------------------------
      // Left Engine and Wing routine
      if (fuelLeftPre > 0) {
        // Cycle Fuel Burn for Engine 1
        if (devState != 2) {
          m = (engine1FF - engine1PreFF) / deltaTime;
          b = engine1PreFF;
          fuelBurn1 = (m * pow(deltaTime, 2) / 2) + (b * deltaTime);  // KG
        }

        // Fuel transfer routine for Left Wing
        if (xfrValveOuterLeft1 > 0.0 || xfrValveOuterLeft2 > 0.0)
          xfrAuxLeft = fuelAuxLeftPre - leftAuxQuantity;
      } else {
        fuelBurn1 = 0;
        fuelLeftPre = 0;
      }

      //--------------------------------------------
      // Right Engine and Wing routine
      if (fuelRightPre > 0) {
        // Cycle Fuel Burn for Engine 2
        if (devState != 2) {
          m = (engine2FF - engine2PreFF) / deltaTime;
          b = engine2PreFF;
          fuelBurn2 = (m * pow(deltaTime, 2) / 2) + (b * deltaTime);  // KG
        }
        // Fuel transfer routine for Right Wing
        if (xfrValveOuterRight1 > 0.0 || xfrValveOuterRight2 > 0.0)
          xfrAuxRight = fuelAuxRightPre - rightAuxQuantity;
      } else {
        fuelBurn2 = 0;
        fuelRightPre = 0;
      }

      /// apu fuel consumption for this frame in pounds
      double apuFuelConsumption = simVars->getLineFlow(18) * fuelWeightGallon * deltaTime;
      apuBurn1 = apuFuelConsumption;
      apuBurn2 = 0;

      //--------------------------------------------
      // Fuel used accumulators
      fuelUsedEngine1 += fuelBurn1;
      fuelUsedEngine2 += fuelBurn2;

      //--------------------------------------------
      // Cross-feed fuel burn routine
      // If fuel pumps for a given tank are closed,
      // all fuel will be burnt on the other tank
      switch (isTankClosed) {
        case 1:
          fuelBurn2 = fuelBurn1 + fuelBurn2;
          fuelBurn1 = 0;
          apuBurn1 = 0;
          apuBurn2 = apuFuelConsumption;
          break;
        case 2:
          fuelBurn1 = fuelBurn1 + fuelBurn2;
          fuelBurn2 = 0;
          break;
        case 3:
          fuelBurn1 = 0;
          fuelBurn2 = 0;
          apuBurn1 = apuFuelConsumption * 0.5;
          apuBurn2 = apuFuelConsumption * 0.5;
          break;
        case 4:
          apuBurn1 = apuFuelConsumption*0.5;
          apuBurn2 = apuFuelConsumption*0.5;
          break;
        default:
          break;
      }

      //--------------------------------------------
      // Center Tank transfer routine
      if (xfrValveCenterLeftOpen && xfrValveCenterRightOpen) {
        if (lineLeftToCenterFlow < 0.1 && lineRightToCenterFlow < 0.1)
          lineFlowRatio = 0.5;
        else
          lineFlowRatio = lineLeftToCenterFlow / (lineLeftToCenterFlow + lineRightToCenterFlow);

        xfrCenterToLeft = (fuelCenterPre - centerQuantity) * lineFlowRatio;
        xfrCenterToRight = (fuelCenterPre - centerQuantity) * (1 - lineFlowRatio);
      } else if (xfrValveCenterLeftOpen)
        xfrCenterToLeft = fuelCenterPre - centerQuantity;
      else if (xfrValveCenterRightOpen)
        xfrCenterToRight = fuelCenterPre - centerQuantity;

      //--------------------------------------------
      // Final Fuel levels for left and right inner tanks
      fuelLeft = (fuelLeftPre - (fuelBurn1 * KGS_TO_LBS)) + xfrAuxLeft + xfrCenterToLeft - apuBurn1;  // LBS
      fuelRight = (fuelRightPre - (fuelBurn2 * KGS_TO_LBS)) + xfrAuxRight + xfrCenterToRight - apuBurn2;                   // LBS

      //--------------------------------------------
      // Setting new pre-cycle conditions
      simVars->setEngine1PreFF(engine1FF);
      simVars->setEngine2PreFF(engine2FF);
      simVars->setFuelUsedEngine1(fuelUsedEngine1);   // in KG
      simVars->setFuelUsedEngine2(fuelUsedEngine2);   // in KG
      simVars->setFuelAuxLeftPre(leftAuxQuantity);    // in LBS
      simVars->setFuelAuxRightPre(rightAuxQuantity);  // in LBS
      simVars->setFuelCenterPre(centerQuantity);      // in LBS

      simVars->setFuelLeftPre(fuelLeft);    // in LBS
      simVars->setFuelRightPre(fuelRight);  // in LBS

      fuelLeft = (fuelLeft / fuelWeightGallon);    // USG
      fuelRight = (fuelRight / fuelWeightGallon);  // USG

      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::FuelLeftMain, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double), &fuelLeft);
      SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::FuelRightMain, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double), &fuelRight);
    }

    //--------------------------------------------
    // Will save the current fuel quantities if on
    // the ground AND engines being shutdown
    if (timerFuel.elapsed() >= 1000 && simVars->getSimOnGround() &&
        (engine1State == 0 || engine1State == 10 || engine1State == 4 || engine1State == 14 || engine2State == 0 || engine2State == 10 ||
         engine2State == 4 || engine2State == 14)) {
      Configuration configuration;

      configuration.fuelLeft = simVars->getFuelLeftPre() / simVars->getFuelWeightGallon();
      configuration.fuelRight = simVars->getFuelRightPre() / simVars->getFuelWeightGallon();
      configuration.fuelCenter = simVars->getFuelCenterPre() / simVars->getFuelWeightGallon();
      configuration.fuelLeftAux = simVars->getFuelAuxLeftPre() / simVars->getFuelWeightGallon();
      configuration.fuelRightAux = simVars->getFuelAuxRightPre() / simVars->getFuelWeightGallon();
      configuration.fuelTrim = simVars->getFuelTrimPre() / simVars->getFuelWeightGallon();

      saveFuelInConfiguration(configuration);
      timerFuel.reset();
    }
  }

  void updateThrustLimits(double simulationTime,
                          double altitude,
                          double ambientTemp,
                          double ambientPressure,
                          double mach,
                          double simN1highest,
                          double packs,
                          double nai,
                          double wai) {
    double idle = simVars->getEngineIdleN1();
    double flexTemp = simVars->getFlexTemp();
    double thrustLimitType = simVars->getThrustLimitType();
    double to = 0;
    double ga = 0;
    double toga = 0;
    double clb = 0;
    double mct = 0;
    double flex_to = 0;
    double flex_ga = 0;
    double flex = 0;

    // Write all N1 Limits
    to = limitN1(0, min(16600.0, pressAltitude), ambientTemp, ambientPressure, 0, packs, nai, wai);
    ga = limitN1(1, min(16600.0, pressAltitude), ambientTemp, ambientPressure, 0, packs, nai, wai);
    if (flexTemp > 0) {
      flex_to = limitN1(0, min(16600.0, pressAltitude), ambientTemp, ambientPressure, flexTemp, packs, nai, wai);
      flex_ga = limitN1(1, min(16600.0, pressAltitude), ambientTemp, ambientPressure, flexTemp, packs, nai, wai);
    }
    clb = limitN1(2, pressAltitude, ambientTemp, ambientPressure, 0, packs, nai, wai);
    mct = limitN1(3, pressAltitude, ambientTemp, ambientPressure, 0, packs, nai, wai);

    // transition between TO and GA limit -----------------------------------------------------------------------------
    double machFactorLow = max(0.0, min(1.0, (mach - 0.04) / 0.04));
    toga = to + (ga - to) * machFactorLow;
    flex = flex_to + (flex_ga - flex_to) * machFactorLow;

    // adaption of CLB due to FLX limit if necessary ------------------------------------------------------------------

    if ((prevThrustLimitType != 3 && thrustLimitType == 3) || (prevFlexTemperature == 0 && flexTemp > 0)) {
      isFlexActive = true;
    } else if ((flexTemp == 0) || (thrustLimitType == 4)) {
      isFlexActive = false;
    }

    if (isFlexActive && !isTransitionActive && thrustLimitType == 1) {
      isTransitionActive = true;
      transitionStartTime = simulationTime;
      transitionFactor = 0.2;
      // transitionFactor = (clb - flex) / transitionTime;
    } else if (!isFlexActive) {
      isTransitionActive = false;
      transitionStartTime = 0;
      transitionFactor = 0;
    }

    double deltaThrust = 0;

    if (isTransitionActive) {
      double timeDifference = max(0, (simulationTime - transitionStartTime) - waitTime);

      if (timeDifference > 0 && clb > flex) {
        deltaThrust = min(clb - flex, timeDifference * transitionFactor);
      }

      if (flex + deltaThrust >= clb) {
        isFlexActive = false;
        isTransitionActive = false;
      }
    }

    if (isFlexActive) {
      clb = min(clb, flex) + deltaThrust;
    }

    prevThrustLimitType = thrustLimitType;
    prevFlexTemperature = flexTemp;

    // thrust transitions for MCT and TOGA ----------------------------------------------------------------------------

    // get factors
    double machFactor = max(0.0, min(1.0, ((mach - 0.37) / 0.05)));
    double altitudeFactorLow = max(0.0, min(1.0, ((altitude - 16600) / 500)));
    double altitudeFactorHigh = max(0.0, min(1.0, ((altitude - 25000) / 500)));

    // adapt thrust limits
    if (altitude >= 25000) {
      mct = max(clb, mct + (clb - mct) * altitudeFactorHigh);
      toga = mct;
    } else {
      if (mct > toga) {
        mct = toga + (mct - toga) * min(1.0, altitudeFactorLow + machFactor);
        toga = mct;
      } else {
        toga = toga + (mct - toga) * min(1.0, altitudeFactorLow + machFactor);
      }
    }

    // write limits ---------------------------------------------------------------------------------------------------
    simVars->setThrustLimitIdle(idle);
    simVars->setThrustLimitToga(toga);
    simVars->setThrustLimitFlex(flex);
    simVars->setThrustLimitClimb(clb);
    simVars->setThrustLimitMct(mct);
  }

 public:
  /// <summary>
  /// Initialize the FADEC and Fuel model
  /// </summary>
  void initialize(const char* acftRegistration) {
    srand((int)time(0));

    std::cout << "FADEC: Initializing EngineControl" << std::endl;

    simVars = new SimVars();
    double engTime = 0;
    ambientTemp = simVars->getAmbientTemperature();
    simN3Engine1Pre = simVars->getN2(1);
    simN3Engine2Pre = simVars->getN2(2);

    confFilename += acftRegistration;
    confFilename += FILENAME_FADEC_CONF_FILE_EXTENSION;

    Configuration configuration = getConfigurationFromFile();

    // One-off Engine imbalance
    generateEngineImbalance(1);
    imbalance = simVars->getEngineImbalance();
    engineImbalanced = imbalanceExtractor(imbalance, 1);

    // Checking engine imbalance
    if (engineImbalanced != engine) {
      paramImbalance = 0;
    }

    for (engine = 1; engine <= 2; engine++) {
      // Obtain Engine Time
      engTime = simVars->getEngineTime(engine) + engTime;

      // Checking engine imbalance
      paramImbalance = imbalanceExtractor(imbalance, 5) / 10;

      if (engineImbalanced != engine) {
        paramImbalance = 0;
      }

      // Engine Idle Oil Qty
      idleOil = initOil(140, 200);

      // Setting initial Oil
      if (engine == 1) {
        simVars->setEngine1TotalOil(idleOil - paramImbalance);
      } else {
        simVars->setEngine2TotalOil(idleOil - paramImbalance);
      }
    }

    // Setting initial Oil Temperature
    thermalEnergy1 = 0;
    thermalEnergy2 = 0;
    oilTemperatureMax = imbalanceExtractor(imbalance, 8);
    simOnGround = simVars->getSimOnGround();
    double engine1Combustion = simVars->getEngineCombustion(1);
    double engine2Combustion = simVars->getEngineCombustion(2);

    if (simOnGround == 1 && engine1Combustion == 1 && engine2Combustion == 1) {
      oilTemperatureEngine1Pre = 75;
      oilTemperatureEngine2Pre = 75;
    } else if (simOnGround == 0 && engine1Combustion == 1 && engine2Combustion == 1) {
      oilTemperatureEngine1Pre = 85;
      oilTemperatureEngine2Pre = 85;

    } else {
      oilTemperatureEngine1Pre = ambientTemp;
      oilTemperatureEngine2Pre = ambientTemp;
    }

    SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::OilTempEngine1, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double),
                                  &oilTemperatureEngine1Pre);
    SimConnect_SetDataOnSimObject(hSimConnect, DataTypesID::OilTempEngine2, SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(double),
                                  &oilTemperatureEngine2Pre);

    // Initialize Engine State
    simVars->setEngine1State(10);
    simVars->setEngine2State(10);

    // Resetting Engine Timers
    simVars->setEngine1Timer(0);
    simVars->setEngine2Timer(0);

    // Initialize Fuel Tanks
    simVars->setFuelLeftPre(configuration.fuelLeft * simVars->getFuelWeightGallon());          // in LBS
    simVars->setFuelRightPre(configuration.fuelRight * simVars->getFuelWeightGallon());        // in LBS
    simVars->setFuelAuxLeftPre(configuration.fuelLeftAux * simVars->getFuelWeightGallon());    // in LBS
    simVars->setFuelAuxRightPre(configuration.fuelRightAux * simVars->getFuelWeightGallon());  // in LBS
    simVars->setFuelCenterPre(configuration.fuelCenter * simVars->getFuelWeightGallon());      // in LBS
    simVars->setFuelTrimPre(configuration.fuelTrim * simVars->getFuelWeightGallon());

    // Initialize Pump State
    simVars->setPumpStateEngine1(0);
    simVars->setPumpStateEngine2(0);

    // Initialize Thrust Limits
    simVars->setThrustLimitIdle(0);
    simVars->setThrustLimitToga(0);
    simVars->setThrustLimitFlex(0);
    simVars->setThrustLimitClimb(0);
    simVars->setThrustLimitMct(0);
  }

  /// <summary>
  /// Update cycle at deltaTime
  /// </summary>
  void update(double deltaTime, double simulationTime) {
    double prevAnimationDeltaTime;
    double simN1highest = 0;

    // animationDeltaTimes being used to detect a Paused situation
    prevAnimationDeltaTime = animationDeltaTime;
    animationDeltaTime = simVars->getAnimDeltaTime();

    mach = simVars->getMach();
    pressAltitude = simVars->getPressureAltitude();
    ambientTemp = simVars->getAmbientTemperature();
    ambientPressure = simVars->getAmbientPressure();
    simOnGround = simVars->getSimOnGround();
    imbalance = simVars->getEngineImbalance();
    packs = 0;
    nai = 0;
    wai = 0;

    // Obtain Bleed Variables
    if (simVars->getPacksState1() > 0.5 || simVars->getPacksState2() > 0.5) {
      packs = 1;
    }
    if (simVars->getNAI(1) > 0.5 || simVars->getNAI(2) > 0.5) {
      nai = 1;
    }
    wai = simVars->getWAI();

    generateIdleParameters(pressAltitude, mach, ambientTemp, ambientPressure);

    // Timer timer;
    for (engine = 1; engine <= 2; engine++) {
      engineStarter = simVars->getEngineStarter(engine);
      engineIgniter = simVars->getEngineIgniter(engine);
      simCN1 = simVars->getCN1(engine);
      simN1 = simVars->getN1(engine);
      simN3 = simVars->getN2(engine);
      thrust = simVars->getThrust(engine);

      // Set & Check Engine Status for this Cycle
      engineStateMachine(engine, engineIgniter, engineStarter, simN3, idleN3, pressAltitude, ambientTemp,
                         animationDeltaTime - prevAnimationDeltaTime);
      if (engine == 1) {
        engineState = simVars->getEngine1State();
        deltaN3 = simN3 - simN3Engine1Pre;
        simN3Engine1Pre = simN3;
        timer = simVars->getEngine1Timer();
      } else {
        engineState = simVars->getEngine2State();
        deltaN3 = simN3 - simN3Engine2Pre;
        simN3Engine2Pre = simN3;
        timer = simVars->getEngine2Timer();
      }

      switch (int(engineState)) {
        case 2:
        case 3:
          engineStartProcedure(engine, engineState, imbalance, deltaTime, timer, simN3, pressAltitude, ambientTemp);
          break;
        case 4:
          engineShutdownProcedure(engine, ambientTemp, simN1, deltaTime, timer);
          cFbwFF = updateFF(engine, imbalance, simCN1, mach, pressAltitude, ambientTemp, ambientPressure);
          break;
        default:
          updatePrimaryParameters(engine, imbalance, simN1, simN3);
          cFbwFF = updateFF(engine, imbalance, simCN1, mach, pressAltitude, ambientTemp, ambientPressure);
          updateEGT(engine, imbalance, deltaTime, simOnGround, engineState, simCN1, cFbwFF, mach, pressAltitude, ambientTemp);
          // updateOil(engine, imbalance, thrust, simN3, deltaN3, deltaTime, ambientTemp);
      }

      // set highest N1 from either engine
      simN1highest = max(simN1highest, simN1);
    }

    updateFuel(deltaTime);

    updateThrustLimits(simulationTime, pressAltitude, ambientTemp, ambientPressure, mach, simN1highest, packs, nai, wai);
    // timer.elapsed();
  }

  void terminate() {}

  Configuration getConfigurationFromFile() {
    Configuration configuration;
    mINI::INIStructure stInitStructure;

    mINI::INIFile iniFile(confFilename);

    if (!iniFile.read(stInitStructure)) {
      std::cout << "EngineControl: failed to read configuration file " << confFilename << " due to error \"" << strerror(errno)
                << "\" -> use default main/aux/center: " << configuration.fuelLeft << "/" << configuration.fuelLeftAux << "/"
                << configuration.fuelCenter << std::endl;
    } else {
      configuration = loadConfiguration(stInitStructure);
    }

    return configuration;
  }

  Configuration loadConfiguration(const mINI::INIStructure& structure) {
    return {
        mINI::INITypeConversion::getDouble(structure, CONFIGURATION_SECTION_FUEL, CONFIGURATION_SECTION_FUEL_CENTER_QUANTITY, 0),
        mINI::INITypeConversion::getDouble(structure, CONFIGURATION_SECTION_FUEL, CONFIGURATION_SECTION_FUEL_LEFT_QUANTITY, 1645.0),
        mINI::INITypeConversion::getDouble(structure, CONFIGURATION_SECTION_FUEL, CONFIGURATION_SECTION_FUEL_RIGHT_QUANTITY, 1645.0),
        mINI::INITypeConversion::getDouble(structure, CONFIGURATION_SECTION_FUEL, CONFIGURATION_SECTION_FUEL_LEFT_AUX_QUANTITY, 228.0),
        mINI::INITypeConversion::getDouble(structure, CONFIGURATION_SECTION_FUEL, CONFIGURATION_SECTION_FUEL_RIGHT_AUX_QUANTITY, 228.0),
        mINI::INITypeConversion::getDouble(structure, CONFIGURATION_SECTION_FUEL, CONFIGURATION_SECTION_FUEL_TRIM_QUANTITY, 1617.0),
    };
  }

  void saveFuelInConfiguration(Configuration configuration) {
    mINI::INIStructure stInitStructure;
    mINI::INIFile iniFile(confFilename);

    // Do not check a possible error since the file may not exist yet
    iniFile.read(stInitStructure);

    stInitStructure[CONFIGURATION_SECTION_FUEL][CONFIGURATION_SECTION_FUEL_CENTER_QUANTITY] = std::to_string(configuration.fuelCenter);
    stInitStructure[CONFIGURATION_SECTION_FUEL][CONFIGURATION_SECTION_FUEL_LEFT_QUANTITY] = std::to_string(configuration.fuelLeft);
    stInitStructure[CONFIGURATION_SECTION_FUEL][CONFIGURATION_SECTION_FUEL_RIGHT_QUANTITY] = std::to_string(configuration.fuelRight);
    stInitStructure[CONFIGURATION_SECTION_FUEL][CONFIGURATION_SECTION_FUEL_LEFT_AUX_QUANTITY] = std::to_string(configuration.fuelLeftAux);
    stInitStructure[CONFIGURATION_SECTION_FUEL][CONFIGURATION_SECTION_FUEL_RIGHT_AUX_QUANTITY] = std::to_string(configuration.fuelRightAux);
    stInitStructure[CONFIGURATION_SECTION_FUEL][CONFIGURATION_SECTION_FUEL_TRIM_QUANTITY] = std::to_string(configuration.fuelTrim);

    if (!iniFile.write(stInitStructure, true)) {
      std::cout << "EngineControl: failed to write engine conf " << confFilename << " due to error \"" << strerror(errno) << "\""
                << std::endl;
    }
  }
};

EngineControl EngineControlInstance;
