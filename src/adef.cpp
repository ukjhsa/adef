#include <memory>
#include <string>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include "adef.h"
#include "Configuration.h"
#include "PrototypeManager.h"

#include "System.h"
#include "SystemStatistics.h"
#include "Experiment.h"
#include "ExperimentalStatistics.h"
#include "Random.h"
#include "Repository.h"

#include "Evolution.h"
#include "Problem.h"
#include "Statistics.h"
#include "Parameters.h"
#include "Initializer.h"
#include "Evaluator.h"
#include "Population.h"
#include "Individual.h"

#include "DEReproduction.h"
#include "DEEnvironmentalSelection.h"
#include "DEMutation.h"
#include "DERand.h"
#include "DEVariantY.h"
#include "DECrossover.h"
#include "DEBinomial.h"
#include "ReflectiveRepair.h"
#include "BoundedRepair.h"

#include "problem/problem.h"
#include "cm/cm.h"

namespace adef {

void register_type(PrototypeManager& pm)
{
    pm.register_type("System", std::make_shared<System>());
    pm.register_type("SystemStatistics", std::make_shared<SystemStatistics>());
    pm.register_type("Experiment", std::make_shared<Experiment>());
    pm.register_type("ExperimentalStatistics",
                     std::make_shared<ExperimentalStatistics>());
    pm.register_type("Random", std::make_shared<Random>());
    pm.register_type("Mt19937Random", std::make_shared<Mt19937Random>());
    pm.register_type("Mt19937_64Random", std::make_shared<Mt19937_64Random>());
    pm.register_type("MinstdRandRandom", std::make_shared<MinstdRandRandom>());
    pm.register_type("Repository", std::make_shared<Repository>());

    pm.register_type("Evolution", std::make_shared<Evolution>());
    pm.register_type("Population", std::make_shared<Population>());
    pm.register_type("Individual", std::make_shared<Individual>());
    pm.register_type("DEMutation", std::make_shared<DEMutation>());
    pm.register_type("DERand", std::make_shared<DERand>());
    pm.register_type("DEVariantY", std::make_shared<DEVariantY>());
    pm.register_type("DECrossover", std::make_shared<DECrossover>());
    pm.register_type("DEBinomial", std::make_shared<DEBinomial>());
    pm.register_type("DEReproduction", std::make_shared<DEReproduction>());
    pm.register_type("ReflectiveRepair", std::make_shared<ReflectiveRepair>());
    pm.register_type("BoundedRepair", std::make_shared<BoundedRepair>());
    pm.register_type("DEEnvironmentalSelection", std::make_shared<DEEnvironmentalSelection>());
    pm.register_type("Parameters", std::make_shared<Parameters>());
    pm.register_type("Evaluator", std::make_shared<Evaluator>());
    pm.register_type("Initializer", std::make_shared<Initializer>());
    pm.register_type("Statistics", std::make_shared<Statistics>());

    pm.register_type("Sphere", std::make_shared<Sphere>());
    pm.register_type("Schwefel2pt22", std::make_shared<Schwefel2pt22>());
    pm.register_type("Schwefel1pt2", std::make_shared<Schwefel1pt2>());
    pm.register_type("Schwefel2pt21", std::make_shared<Schwefel2pt21>());
    pm.register_type("Rosenbrock", std::make_shared<Rosenbrock>());
    pm.register_type("Step", std::make_shared<Step>());
    pm.register_type("QuarticFunctionWithNoise", std::make_shared<QuarticFunctionWithNoise>());
    pm.register_type("GeneralizedSchwefel2pt26", std::make_shared<GeneralizedSchwefel2pt26>());
    pm.register_type("Rastrigin", std::make_shared<Rastrigin>());
    pm.register_type("Ackley", std::make_shared<Ackley>());
    pm.register_type("Griewank", std::make_shared<Griewank>());
    pm.register_type("GeneralizedPenalizedNo1", std::make_shared<GeneralizedPenalizedNo1>());
    pm.register_type("GeneralizedPenalizedNo2", std::make_shared<GeneralizedPenalizedNo2>());
    pm.register_type("ShekelFoxholes", std::make_shared<ShekelFoxholes>());
    pm.register_type("Kowalik", std::make_shared<Kowalik>());
    pm.register_type("SixHumpCamelBack", std::make_shared<SixHumpCamelBack>());
    pm.register_type("Branin", std::make_shared<Branin>());
    pm.register_type("GoldsteinPrice", std::make_shared<GoldsteinPrice>());
    pm.register_type("HartmanNo1", std::make_shared<HartmanNo1>());
    pm.register_type("HartmanNo2", std::make_shared<HartmanNo2>());
    pm.register_type("ShekelNo1", std::make_shared<ShekelNo1>());
    pm.register_type("ShekelNo2", std::make_shared<ShekelNo2>());
    pm.register_type("ShekelNo3", std::make_shared<ShekelNo3>());

    pm.register_type("BentCigar", std::make_shared<BentCigar>());
    pm.register_type("Discus", std::make_shared<Discus>());
    pm.register_type("HGBat", std::make_shared<HGBat>());
    pm.register_type("HappyCat", std::make_shared<HappyCat>());
    pm.register_type("Katsuura", std::make_shared<Katsuura>());

    pm.register_type("IntegerControlMechanism", std::make_shared<IntegerControlMechanism>());
    pm.register_type("RealControlMechanism", std::make_shared<RealControlMechanism>());
    pm.register_type("IntegerControlControlMechanism", std::make_shared<IntegerControlControlMechanism>());
    pm.register_type("RealControlControlMechanism", std::make_shared<RealControlControlMechanism>());
    pm.register_type("SadeCrControlMechanism", std::make_shared<SadeCrControlMechanism>());
    pm.register_type("SdeFControlMechanism", std::make_shared<SdeFControlMechanism>());
    pm.register_type("IntegerIndirectControlMechanism", std::make_shared<IntegerIndirectControlMechanism>());
    pm.register_type("RealIndirectControlMechanism", std::make_shared<RealIndirectControlMechanism>());

    pm.register_type("IntegerControlRange", std::make_shared<IntegerControlRange>());
    pm.register_type("RealControlRange", std::make_shared<RealControlRange>());
    pm.register_type("IntegerControlControlRange", std::make_shared<IntegerControlControlRange>());
    pm.register_type("RealControlControlRange", std::make_shared<RealControlControlRange>());

    pm.register_type("IntegerSingleControlParameter", std::make_shared<IntegerSingleControlParameter>());
    pm.register_type("RealSingleControlParameter", std::make_shared<RealSingleControlParameter>());
    pm.register_type("IntegerControlSingleControlParameter", std::make_shared<IntegerControlSingleControlParameter>());
    pm.register_type("RealControlSingleControlParameter", std::make_shared<RealControlSingleControlParameter>());
    pm.register_type("IntegerMultipleControlParameter", std::make_shared<IntegerMultipleControlParameter>());
    pm.register_type("RealMultipleControlParameter", std::make_shared<RealMultipleControlParameter>());
    pm.register_type("IntegerControlMultipleControlParameter", std::make_shared<IntegerControlMultipleControlParameter>());
    pm.register_type("RealControlMultipleControlParameter", std::make_shared<RealControlMultipleControlParameter>());

    pm.register_type("SingleControlFunction", std::make_shared<SingleControlFunction>());
    pm.register_type("MultipleControlFunction", std::make_shared<MultipleControlFunction>());

    pm.register_type("NonInfoControlSelection", std::make_shared<NonInfoControlSelection>());
    pm.register_type("IntegerBetterOffspringControlSelection", std::make_shared<IntegerBetterOffspringControlSelection>());
    pm.register_type("RealBetterOffspringControlSelection", std::make_shared<RealBetterOffspringControlSelection>());
    pm.register_type("IntegerControlBetterOffspringControlSelection", std::make_shared<IntegerControlBetterOffspringControlSelection>());
    pm.register_type("RealControlBetterOffspringControlSelection", std::make_shared<RealControlBetterOffspringControlSelection>());

    pm.register_type("AverageFitnessControlUpdate", std::make_shared<AverageFitnessControlUpdate>());
    pm.register_type("CurrentFitnessControlUpdate", std::make_shared<CurrentFitnessControlUpdate>());
    pm.register_type("GenerationControlUpdate", std::make_shared<GenerationControlUpdate>());
    pm.register_type("MaxFitnessControlUpdate", std::make_shared<MaxFitnessControlUpdate>());
    pm.register_type("MinFitnessControlUpdate", std::make_shared<MinFitnessControlUpdate>());
    pm.register_type("SdeFControlUpdate", std::make_shared<SdeFControlUpdate>());

    pm.register_type("RealCauchyDisFunction", std::make_shared<RealCauchyDisFunction>());
    pm.register_type("IntegerConstantFunction", std::make_shared<IntegerConstantFunction>());
    pm.register_type("RealConstantFunction", std::make_shared<RealConstantFunction>());
    pm.register_type("IntegerControlConstantFunction", std::make_shared<IntegerControlConstantFunction>());
    pm.register_type("RealControlConstantFunction", std::make_shared<RealControlConstantFunction>());
    pm.register_type("DepdFFunction", std::make_shared<DepdFFunction>());
    pm.register_type("IsadeCrFunction", std::make_shared<IsadeCrFunction>());
    pm.register_type("IsadeFFunction", std::make_shared<IsadeFFunction>());
    pm.register_type("JdeCrFunction", std::make_shared<JdeCrFunction>());
    pm.register_type("JdeFFunction", std::make_shared<JdeFFunction>());
    pm.register_type("IntegerLearningPeriodFunction", std::make_shared<IntegerLearningPeriodFunction>());
    pm.register_type("RealLearningPeriodFunction", std::make_shared<RealLearningPeriodFunction>());
    pm.register_type("IntegerControlLearningPeriodFunction", std::make_shared<IntegerControlLearningPeriodFunction>());
    pm.register_type("RealControlLearningPeriodFunction", std::make_shared<RealControlLearningPeriodFunction>());
    pm.register_type("IntegerMedianFunction", std::make_shared<IntegerMedianFunction>());
    pm.register_type("RealMedianFunction", std::make_shared<RealMedianFunction>());
    pm.register_type("RealNormalDisFunction", std::make_shared<RealNormalDisFunction>());
    pm.register_type("IntegerRandomSelectionFunction", std::make_shared<IntegerRandomSelectionFunction>());
    pm.register_type("RealRandomSelectionFunction", std::make_shared<RealRandomSelectionFunction>());
    pm.register_type("IntegerControlRandomSelectionFunction", std::make_shared<IntegerControlRandomSelectionFunction>());
    pm.register_type("RealControlRandomSelectionFunction", std::make_shared<RealControlRandomSelectionFunction>());
    pm.register_type("IntegerRouletteWheelSelectionFunction", std::make_shared<IntegerRouletteWheelSelectionFunction>());
    pm.register_type("RealRouletteWheelSelectionFunction", std::make_shared<RealRouletteWheelSelectionFunction>());
    pm.register_type("IntegerControlRouletteWheelSelectionFunction", std::make_shared<IntegerControlRouletteWheelSelectionFunction>());
    pm.register_type("RealControlRouletteWheelSelectionFunction", std::make_shared<RealControlRouletteWheelSelectionFunction>());
    pm.register_type("SdeFFunction", std::make_shared<SdeFFunction>());
    pm.register_type("IntegerUniformDisFunction", std::make_shared<IntegerUniformDisFunction>());
    pm.register_type("RealUniformDisFunction", std::make_shared<RealUniformDisFunction>());
    pm.register_type("IntegerVariableFunction", std::make_shared<IntegerVariableFunction>());
    pm.register_type("RealVariableFunction", std::make_shared<RealVariableFunction>());
    pm.register_type("IntegerControlVariableFunction", std::make_shared<IntegerControlVariableFunction>());
    pm.register_type("RealControlVariableFunction", std::make_shared<RealControlVariableFunction>());
    pm.register_type("WeightedAverageFunction", std::make_shared<WeightedAverageFunction>());

    pm.register_type("SuccessScoringFunction", std::make_shared<SuccessScoringFunction>());
    pm.register_type("ImprovedPercentageScoringFunction", std::make_shared<ImprovedPercentageScoringFunction>());
}

std::shared_ptr<System> init_adef(int argc, char *argv[])
{
    PrototypeManager pm;
    register_type(pm);

    Configuration config;
    if (argc == 3) {
        auto result = std::strncmp(argv[1], "-file", 5);
        if (result != 0) {
            throw std::runtime_error("command should be <-file> <file_name>");
        }
        if (!config.load_config(argv[2])) {
            std::string message = "Load error on file: ";
            throw std::runtime_error(message + argv[2]);
        }
    }
    else {
        std::string config_file = "config.json";
        std::cout << "Load default configuration: " << config_file << std::endl;
        if (!config.load_config(config_file)) {
            throw std::runtime_error("Load error on file: " + config_file);
        }
    }

    return make_and_setup_type<System>("System", config, pm);;
}

}
