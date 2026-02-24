/**
 * @brief Strong Types related to the motion system
 **/
#pragma once
#include <core/types.h>

#if ENABLED(COREXY)
struct CoordCoreXYTag {};
typedef CoordCoreXYTag CoordTypeTag;

struct StepsCoreXYTag {};
typedef StepsCoreXYTag StepsTypeTag;

struct MStepsCoreXYTag {};
typedef MStepsCoreXYTag MStepsTypeTag;
#else // CARTESIAN
struct CoordCartTag {};
typedef CoordCartTag CoordTypeTag;

struct StepsCartTag {};
typedef StepsCartTag StepsTypeTag;

struct MStepsCartTag {};
typedef MStepsCartTag MStepsTypeTag;
#endif

// AB logical positions
typedef struct XYval<float, CoordTypeTag> ab_pos_t;
typedef struct XYZval<float, CoordTypeTag> abc_pos_t;
typedef struct XYZEval<float, CoordTypeTag> abce_pos_t;

// AB positions in steps
typedef struct XYval<long, StepsTypeTag> ab_steps_t;
typedef struct XYZval<long, StepsTypeTag> abc_steps_t;
typedef struct XYZEval<long, StepsTypeTag> abce_steps_t;

// AB positions in mini-steps
typedef struct XYval<long, MStepsTypeTag> ab_msteps_t;
typedef struct XYZval<long, MStepsTypeTag> abc_msteps_t;
typedef struct XYZEval<long, MStepsTypeTag> abce_msteps_t;
