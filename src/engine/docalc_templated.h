
/* Hello reader!

 * Are you sure you want read this? Its very cryptic and strange code. YOU
 * HAVE BEEN WARNED! Its purpose is to generate as fast as possible
 * calculation loops for various formulas/algorithms. It uses lots of
 * template magic. It is included from formulas.c
 */
#include "config.h"
#include "fractal.h"


// Just to select between smooth or nonsmoothmodes
enum class SmoothMode {
    SmoothEnabled, SmoothDisabled
};


// Iterates Z until end condition is reached
template<class Formula, SmoothMode SmoothModeSelected>
class DefaultLoop {
public:
    inline static void IterateUntilEnd(unsigned int &iter, typename Formula::Variables &vars)  {
        do {
            Formula::DoOneIterationStep(vars);
            iter--;
        } while (Formula::BailoutTest(vars) && iter) ;
    }
};

// Iterates Z until end condition is reached, stores previous
// Magnitude of Z for smooth color caltulation
// Hint: DefaultSmoothLoop and DefaultLoop might be combined
//       using constexpr-if(smooth)
template<class Formula>
class DefaultLoop<Formula, SmoothMode::SmoothEnabled> {
public:
    inline static void IterateUntilEnd(unsigned int &iter, typename Formula::Variables &vars)  {
        do {
            vars.SaveMagnitudeOfZ();
            Formula::DoOneIterationStep(vars);
            iter--;
        } while (Formula::BailoutTest(vars) && iter) ;
    }
};


// Variables used in all Formulas, will be extended
// for each formula with additional variables as required
template<SmoothMode SmoothModeSelected, class ValueType = number_t>
class CommonVariables {

};

// Common set for non-smoothing-case
template<class ValueT>
class CommonVariables<SmoothMode::SmoothDisabled, ValueT> {
public:
    typedef ValueT ValueType;
    ValueType &zre;
    ValueType &zim;
    ValueType &pre;
    ValueType &pim;

    CommonVariables(ValueType &_zre, ValueType &_zim, ValueType &_pre, ValueType &_pim)
        : zre(_zre), zim(_zim), pre(_pre), pim(_pim) {}
    void Save() {}
    void Restore() {}

    inline void SaveMagnitudeOfZ() {}

};

// Common set for smoothing-case
template<class ValueType>
class CommonVariables<SmoothMode::SmoothEnabled, ValueType> : public CommonVariables<SmoothMode::SmoothDisabled, ValueType> {
public:
    // reuse baseclass constructor
    using CommonVariables<SmoothMode::SmoothDisabled, ValueType>::CommonVariables;

    ValueType szmag = 0;

};


template<template <class, SmoothMode> class FormulaT, SmoothMode SmoothModeSelected, template<class> class Colorizer, class Looper = DefaultLoop<FormulaT<CommonVariables<SmoothModeSelected, number_t>, SmoothModeSelected>, SmoothModeSelected>>
static unsigned int calc(number_t zre, number_t zim, number_t pre,
                         number_t pim)
{
    // Some shortcuts for easier coding
    typedef FormulaT<CommonVariables<SmoothModeSelected, number_t>, SmoothModeSelected> Formula;
    typedef Colorizer<Formula> ColorConverter;

    // Define our Variables
    unsigned int iter = cfractalc.maxiter;
    typename Formula::Variables vars(zre, zim, pre, pim);
    Formula::Init(vars); // cannot be part of costructor of variables, as also used inside code independet of construction (but could be called from constructor, but i think it would be confusing)


    // Select compressed or uncompressed variant
    if constexpr (Formula::DoCompress) {
        // Compressed

        //VARIABLES;
        //INIT;
        // Check if we can skip this Iteration
        if (Formula::Pretest(vars))
            iter = 0;
        else {
            // Prepare Variables for iteration
            Formula::PresetVariablesForFirstIterationStep(vars);

            // Do the whole iteration
            if (Formula::BailoutTest(vars) && iter) {
                Looper::IterateUntilEnd(iter, vars);
            }
        }
    } else {
        // Uncompressed
        number_t szre = 0, szim = 0;

        //VARIABLES;
        //INIT;
        // Check if we can skip this Iteration
        if (Formula::Pretest(vars))
            iter = 0;
        else {
            // Prepare Variables for iteration
            Formula::PresetVariablesForFirstIterationStep(vars);
            if (iter < 16) {
                // For small iterstion limit just iterate
                if (Formula::BailoutTest(vars) && iter) {
                    Looper::IterateUntilEnd(iter, vars);
                }

            } else {

                // Splitt  iterationslimit into a multiple of
                // 8 and a left over
                iter = 8 + (cfractalc.maxiter & 7);

                // iterate the left over part
                if (Formula::BailoutTest(vars) && iter) {
                    Looper::IterateUntilEnd(iter, vars);
                }

                // Now the left over iteraions are a multiple of 8
                if (Formula::BailoutTest(vars)) {

                    // Set iterate count to the left over multiple of 8
                    iter = (cfractalc.maxiter - 8) & (~7);
                    iter >>= 3;

                    /*do next 8 iteration w/o out of bounds checking */
                    do {
                        /*hmm..we are probably in some deep area. */
                        szre = vars.zre; /*save current position */
                        szim = vars.zim;
                        vars.Save();
                        Formula::DoOneIterationStepUncompressed(vars);
                        Formula::DoOneIterationStepUncompressed(vars);
                        Formula::DoOneIterationStepUncompressed(vars);
                        Formula::DoOneIterationStepUncompressed(vars);
                        Formula::DoOneIterationStepUncompressed(vars);
                        Formula::DoOneIterationStepUncompressed(vars);
                        Formula::DoOneIterationStepUncompressed(vars);
                        Formula::DoOneIterationStepUncompressed(vars);
                        Formula::EndUncompressedIteration(vars);
                        iter--;
                    } while (Formula::BailoutTest(vars) && iter);

                    // Redo last 8 iterations if required
                    if (!(Formula::BailoutTest(vars))) { /*we got out of bounds */

                        // Set iterationcount to start of last 8 block
                        iter <<= 3;
                        iter += 8;

                        /*restore saved position */
                        vars.Restore();
                        vars.zre = szre;
                        vars.zim = szim;

                        // Prepare Variables for iteration
                        Formula::PresetVariablesForFirstIterationStep(vars);

                        // and do final iterations
                        Looper::IterateUntilEnd(iter, vars);

                    }
                } else
                    // fix iterationcount to correct value
                    iter += cfractalc.maxiter - 8 - (cfractalc.maxiter & 7);
            }
        }
    }
    if constexpr (SmoothModeSelected == SmoothMode::SmoothEnabled) {

        // Use smooth color calcualation
        if (iter) {
            return ColorConverter::GetSmoothOutColor(vars, iter);
        }
        Formula::PostIterationCalculations(vars, iter);
        iter = cfractalc.maxiter - iter;
        return ColorConverter::GetInColor(vars, iter);
    } else {

        // Use standard color calcualation
        Formula::PostIterationCalculations(vars, iter);
        iter = cfractalc.maxiter - iter;
        return ColorConverter::GetColor(vars, iter);
    }
}

/*F. : Periodicity checking routines.          (16-02-97)
   All comments preceded by F. are mine (Fabrice Premel premelfa@etu.utc.fr).
   Tried to make code as efficient as possible.
   Next to do is convert lim in a variable that would be updated sometimes
   I'll try to make here a short explanation on periodicity checking :
   first, we'll define 2 variables : whentosave and whenincsave, which are,
   respectively, a measure of when we have to update saved values to be checked,
   and when to increase interval between 2 updates, as if they're too close,
   we'll miss large periods. We save Z at the beginning, and then we compare
   each new iteration with this Z, and if naerly equal, we declare the suite to
   be periodic. When ( iter mod whentosave ) == 0, we store a new value, and we
   repeat.

   UNCOMPRESSed form is just an extension, with careful that if we only check
   whentosave all 8 iterations, number of iterations must be well set at the
   beginning.This is done by adding a (iter&7) in the while statement preceding
   then uncompressed calculation. */

/*F. : This is from then lim factor that depends all periodicity check spped :
   the bigger it is, the faster we can detect periodicity, but the bigger it is,
   the more we can introduce errors. I suggest a value of
   (maxx-minx)/(double)getmaxx for a classic Mandelbrot Set, and maybe a lesser
   value for an extra power Mandelbrot. But this should be calculated outer
   from here (ie each frame, for example), to avoid new calculus */


#define PCHECK                                                                 \
(abs_less_than(r1 - zre, cfractalc.periodicity_limit) &&                   \
 abs_less_than(s1 - zim, cfractalc.periodicity_limit))


template<template <class, SmoothMode> class FormulaT, SmoothMode SmoothModeSelected, template<class
                                                                     > class Colorizer, class Looper = DefaultLoop<FormulaT<CommonVariables<SmoothModeSelected, number_t>, SmoothModeSelected>, SmoothModeSelected>>
static unsigned int peri(number_t zre, number_t zim, number_t pre, number_t pim)
{
    // Some shortcuts for easier coding
    typedef FormulaT<CommonVariables<SmoothModeSelected, number_t>, SmoothModeSelected> Formula;
    typedef Colorizer<Formula> ColorConverter;

    // Define our Variables
    unsigned int iter = cfractalc.maxiter /*& (~(int) 3) */;
    number_t r1, s1;
    int whensavenew, whenincsave;
    typename Formula::Variables vars(zre, zim, pre, pim);
    Formula::Init(vars); // cannot be part of costructor of variables, as also used inside code independet of construction (but could be called from constructor, but i think it would be confusing)


    // Select compressed or uncompressed variant
    if constexpr (Formula::DoCompress) {
        unsigned int iter1 = 8;

    //VARIABLES;
    //INIT;
    // Check if we can skip this Iteration
    if (Formula::Pretest(vars))
        iter = 0;
    else {
        // Prepare Variables for iteration
        Formula::PresetVariablesForFirstIterationStep(vars);

        // Do the whole iteration
        if (iter < iter1)
            iter1 = iter; iter = 8;

        /*H. : do first few iterations w/o checking */
        // Do the whole iteration
        if (Formula::BailoutTest(vars) && iter1) {
            Looper::IterateUntilEnd(iter1, vars);
        }

        if (iter1) {
            if (iter >= 8)
                iter -= 8 - iter1;
            goto end;
        }
        if (iter <= 8) {
            iter = iter1;
        } else {
            iter -= 8;
            r1 = vars.zre;
            s1 = vars.zim;
            whensavenew = 3; /*You should adapt these values */
            /*F. : We should always define whensavenew as 2^N-1, so we could use
             * a AND instead of % */

            whenincsave = 10;
            /*F. : problem is that after deep zooming, peiodicity is never
               detected early, cause is is quite slow before going in a periodic
               loop. So, we should start checking periodicity only after some
               times */
            while (Formula::BailoutTest(vars) && iter) {
                vars.SaveMagnitudeOfZ();
                Formula::DoOneIterationStep(vars);
                if ((iter & whensavenew) == 0) { /*F. : changed % to & */
                    r1 = vars.zre;
                    s1 = vars.zim;
                    whenincsave--;
                    if (!whenincsave) {
                        whensavenew =
                            ((whensavenew + 1) << 1) -
                            1; /*F. : Changed to define a new AND mask */
                        whenincsave = 10;
                    }
                } else {
                    if (PCHECK) {
                        return ColorConverter::_PERIINOUTPUT();
                    }
                }
                iter--;
            }
        }
    }
    end:;
} else {

    r1 = vars.zre; s1 = vars.zim;
    number_t szre = 0, szim = 0; /*F. : Didn't declared register, cause they are few used */

    //SAVEVARIABLES
    //VARIABLES;
    //INIT;
    if (Formula::Pretest(vars))
        iter = 0;
    else {
        if (cfractalc.maxiter <= 16) {
            // Prepare Variables for iteration
            Formula::PresetVariablesForFirstIterationStep(vars);

            /*F. : Added iter&7 to be sure we'll be on a 8 multiple */
            if (Formula::BailoutTest(vars) && iter) {
                Looper::IterateUntilEnd(iter, vars);
            }

        } else {
            whensavenew = 7; /*You should adapt these values */
            /*F. : We should always define whensavenew as 2^N-1, so we could use
             * a AND instead of % */

            whenincsave = 10;

            // Prepare Variables for iteration
            Formula::PresetVariablesForFirstIterationStep(vars);

            /*F. : problem is that after deep zooming, peiodicity is never
               detected early, cause is is quite slow before going in a periodic
               loop. So, we should start checking periodicity only after some
               times */
            iter = 8 + (cfractalc.maxiter & 7);
            while (Formula::BailoutTest(vars) && iter) { /*F. : Added iter&7 to be sure we'll be on a
                                       8 multiple */
                    vars.SaveMagnitudeOfZ();
                    Formula::DoOneIterationStep(vars);
                iter--;
            }
            if (Formula::BailoutTest(vars)) { /*F. : BTEST is calculed two times here, isn't it ? */
                /*H. : No gcc is clever and adds test to the end :) */
                iter = (cfractalc.maxiter - 8) & (~7);
                do {
                    szre = vars.zre; szim = vars.zim;
                    vars.Save();
                    vars.SaveMagnitudeOfZ();
                        Formula::DoOneIterationStep(vars); /*F. : Calculate one time */
                    if (PCHECK)
                        goto periodicity;
                    Formula::DoOneIterationStep(vars);
                    if (PCHECK)
                        goto periodicity;
                    Formula::DoOneIterationStep(vars);
                    if (PCHECK)
                        goto periodicity;
                    Formula::DoOneIterationStep(vars);
                    if (PCHECK)
                        goto periodicity;
                    Formula::DoOneIterationStep(vars);
                    if (PCHECK)
                        goto periodicity;
                    Formula::DoOneIterationStep(vars);
                    if (PCHECK)
                        goto periodicity;
                    Formula::DoOneIterationStep(vars);
                    if (PCHECK)
                        goto periodicity;
                    Formula::DoOneIterationStep(vars);
                    if (PCHECK)
                        goto periodicity;
                    iter -= 8;
                    /*F. : We only test this now, as it can't be true before */
                    if ((iter & whensavenew) == 0) { /*F. : changed % to & */
                        r1 = vars.zre, s1 = vars.zim;          /*F. : Save new values */
                        whenincsave--;
                        if (!whenincsave) {
                            whensavenew =
                                ((whensavenew + 1) << 1) -
                                1; /*F. : Changed to define a new AND mask */
                            whenincsave = 10; /*F. : Start back */
                        }
                    }
                } while (Formula::BailoutTest(vars) && iter);
                if (!Formula::BailoutTest(vars)) {  /*we got out of bounds */
                    iter += 8; /*restore saved position */
                    vars.Restore();
                    vars.zre = szre;
                    vars.zim = szim;

                    // Prepare Variables for iteration
                    Formula::PresetVariablesForFirstIterationStep(vars);

                    Looper::IterateUntilEnd(iter, vars);

                }
            } else
                iter += cfractalc.maxiter - 8 - (cfractalc.maxiter & 7);
        }
    }

}
if constexpr (SmoothModeSelected == SmoothMode::SmoothEnabled) {
    if (iter)
        return ColorConverter::GetSmoothOutColor(vars, iter);
    Formula::PostIterationCalculations(vars, iter);
    iter = cfractalc.maxiter - iter;
    return ColorConverter::GetInColor(vars, iter);
} else {
    Formula::PostIterationCalculations(vars, iter);
    iter = cfractalc.maxiter - iter;
    return ColorConverter::GetColor(vars, iter);
}
periodicity:
    return ColorConverter::_PERIINOUTPUT();

}

template<template <class, SmoothMode> class FormulaT, int Range, template<class> class Colorizer>
static void julia(struct image *image, number_t pre, number_t pim)
{
    // Some shortcuts for easier coding
    typedef FormulaT<CommonVariables<SmoothMode::SmoothDisabled, number_t>, SmoothMode::SmoothDisabled> Formula;
    typedef Colorizer<Formula> ColorConverter;

    number_t zim, zre;
    typename Formula::Variables vars(zre, zim, pre, pim);

    int i, i1, i2, j, x, y;
    unsigned char iter, itmp2, itmp;
    number_t im, xdelta, ydelta, range, rangep;
    number_t xstep, ystep;
    unsigned char *queue[QMAX];
    unsigned char **qptr;
    unsigned char *addr, **addr1 = image->currlines;
#ifdef STATISTICS
    int guessed = 0, unguessed = 0, iters = 0;
#endif
    //VARIABLES;
    range = (number_t)Range;
    rangep = range * range;

    xdelta = image->width / (RMAX - RMIN);
    ydelta = image->height / (IMAX - IMIN);
    xstep = (RMAX - RMIN) / image->width;
    ystep = (IMAX - IMIN) / image->height;
    init_julia(image, rangep, range, xdelta, ystep);
    for (i2 = 0; i2 < 2; i2++)
        for (i1 = 0; i1 < image->height; i1++) {
            if (i1 % 2)
                i = image->height / 2 - i1 / 2;
            else
                i = image->height / 2 + i1 / 2 + 1;
            if (i >= image->height)
                continue;
            im = IMIN + (i + 0.5) * ystep;
            for (j = (i + i2) & 1; j < image->width; j += 2) {
                STAT(total2++);
                addr = addr1[i] + j;
                if (*addr != NOT_CALCULATED)
                    continue;
                x = j;
                y = i;
                if (y > 0 && y < image->height - 1 && *(addr + 1) && x > 0 &&
                    x < image->width - 1) {
                    if ((iter = *(addr + 1)) != NOT_CALCULATED &&
                        iter == *(addr - 1) && iter == addr1[y - 1][x] &&
                        iter == addr1[y + 1][x]) {
                        *addr = *(addr + 1);
                        continue;
                    }
                }
                vars.zim = im;
                vars.zre = RMIN + (j + 0.5) * xstep;
                iter = (unsigned char)0;
                qptr = queue;
                vars.ip = (vars.zim * vars.zim); // Does the same as preset
                vars.rp = (vars.zre * vars.zre); // but preset is configurable
                //FORMULA::PresetVariablesForFirstIterationStep(vars);// Therefore do not usde preset
                Formula::Init(vars);
                while (1) {
                    if (*addr != NOT_CALCULATED
#ifdef SAG
                        && (*addr == INPROCESS ||
                            (*addr != (unsigned char)1 &&
                             (itmp2 = *(addr + 1)) != NOT_CALCULATED &&
                             ((itmp2 != (itmp = *(addr - 1)) &&
                               itmp != NOT_CALCULATED) ||
                              (itmp2 != (itmp = *((addr1[y + 1]) + x)) &&
                               itmp != NOT_CALCULATED) ||
                              (itmp2 != (itmp = *((addr1[y - 1]) + x)) &&
                               itmp != NOT_CALCULATED))))
#endif
                        ) {
                        if (*addr == INPROCESS || *addr == INSET) {
                            *qptr = addr;
                            qptr++;
                            STAT(guessed++);
                            goto inset;
                        }
                        STAT(guessed++);
                        iter = *addr;
                        goto outset;
                    }
#ifdef STATISTICS
                    if (*addr != NOT_CALCULATED)
                        unguessed++;
#endif
                    if (*addr != INPROCESS) {
                        *qptr = addr;
                        qptr++;
                        *addr = INPROCESS;
                        if (qptr >= queue + QMAX)
                            goto inset;
                    }
                    STAT(iters++);
                    Formula::DoOneIterationStep(vars);
                    vars.ip = (vars.zim * vars.zim); // Does the same as preset
                    vars.rp = (vars.zre * vars.zre); // but preset is configurable
                    //FORMULA::PresetVariablesForFirstIterationStep(vars);// Therefore do not use preset
                    if (greater_than(vars.rp + vars.ip, Range) || !(Formula::BailoutTest(vars)))
                        goto outset;
                    x = (int)((vars.zre - RMIN) * xdelta);
                    y = (int)((vars.zim - IMIN) * ydelta);
                    addr = addr1[y] + x;
                    if ((itmp = *(addr + 1)) != NOT_CALCULATED &&
                        itmp == *(addr - 1) && itmp == addr1[y - 1][x] &&
                        itmp == addr1[y + 1][x]) {
                        *addr = *(addr + 1);
                    }
                }
            inset:
                while (qptr > queue) {
                    qptr--;
                    **qptr = INSET;
                }
                continue;
            outset:
                y = image->palette->size;
                while (qptr > queue) {
                    qptr--;
                    iter++;
                    if ((int)iter >= y)
                        iter = (unsigned char)1;
                    **qptr = iter;
                }
            }
        }
#ifdef STATISTICS
    printf("guessed %i, unguessed %i, iterations %i\n", guessed, unguessed,
           iters);
    guessed2 += guessed;
    unguessed2 += unguessed;
    iters2 += iters;
#endif
}

