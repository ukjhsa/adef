/**
\file
\brief All classes related to
%ControlMechanism, %ControlRange, %ControlParameter, and %Function.

These class implements the parameter control mechanism:
- ControlMechanism
- ControlRange
- ControlParameter
- Function

ControlMechanism has the following class:
- NonInfoControlMechanism
    - DepdFControlMechanism
- IndividualControlMechanism
    - IsadeControlMechanism
    - JdeControlMechanism
    - SdeFControlMechanism
- MultiValueControlMechanism
- IndirectControlMechanism

ControlRange has the following class:
- ControlRange

ControlParameter has the following class:
- SingleControlParameter
- MultipleControlParameter

Function has the following class:
- ConstantFunction
- VariableFunction
- RandomSelectionFunction
- GatherInfoFunction
    - MedianFunction
- ScoredFunction
    - RouletteWheelSelectionFunction
    - WeightedAverageFunction
- CompositeFunction
    - LearningPeriodFunction
    - UniformDisFunction
    - NormalDisFunction
    - CauchyDisFunction
    - DepdFFunction
    - IsadeFFunction
    - IsadeCrFunction
    - JdeFFunction
    - JdeCrFunction
    - SdeFFunction

For graphical representation of class inheritance and collaboration,
please see the documentation of each class.
*/

#include "ControlMechanism.h"
#include "SadeCrControlMechanism.h"
#include "SdeFControlMechanism.h"
#include "IndirectControlMechanism.h"

#include "cr/ControlRange.h"

#include "cp/MultipleControlParameter.h"
#include "cp/SingleControlParameter.h"

#include "cf/SingleControlFunction.h"
#include "cf/MultipleControlFunction.h"

#include "cs/AllOffspringControlSelection.h"
#include "cs/BetterOffspringControlSelection.h"
#include "cs/NonInfoControlSelection.h"

#include "cu/AverageFitnessControlUpdate.h"
#include "cu/CurrentFitnessControlUpdate.h"
#include "cu/GenerationControlUpdate.h"
#include "cu/MaxFitnessControlUpdate.h"
#include "cu/MinFitnessControlUpdate.h"
#include "cu/SdeFControlUpdate.h"

#include "cf/func/CauchyDisFunction.h"
#include "cf/func/ConstantFunction.h"
#include "cf/func/Function.h"
#include "cf/func/DepdFFunction.h"
#include "cf/func/GatherInfoFunction.h"
#include "cf/func/IsadeCrFunction.h"
#include "cf/func/IsadeFFunction.h"
#include "cf/func/JdeCrFunction.h"
#include "cf/func/JdeFFunction.h"
#include "cf/func/LearningPeriodFunction.h"
#include "cf/func/MedianFunction.h"
#include "cf/func/NormalDisFunction.h"
#include "cf/func/RandomSelectionFunction.h"
#include "cf/func/RouletteWheelSelectionFunction.h"
#include "cf/func/ScoredFunction.h"
#include "cf/func/SdeFFunction.h"
#include "cf/func/RouletteWheelSelectionFunction.h"
#include "cf/func/UniformDisFunction.h"
#include "cf/func/VariableFunction.h"
#include "cf/func/WeightedAverageFunction.h"

#include "cf/func/sf/ImprovedPercentageScoringFunction.h"
#include "cf/func/sf/ScoringFunction.h"
#include "cf/func/sf/SuccessScoringFunction.h"
