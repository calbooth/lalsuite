/*
 *  LALInferenceMCMC.c:  Bayesian Followup function testing site
 *
 *  Copyright (C) 2011 Ilya Mandel, Vivien Raymond, Christian Roever,
 *  Marc van der Sluys, John Veitch and Will M. Farr
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 */


#include <stdio.h>
#include <lal/Date.h>
#include <lal/GenerateInspiral.h>
#include <lal/LALInference.h>
#include <lal/FrequencySeries.h>
#include <lal/Units.h>
#include <lal/StringInput.h>
#include <lal/LIGOLwXMLInspiralRead.h>
#include <lal/TimeSeries.h>
#include "LALInferenceMCMCSampler.h"
#include <lal/LALInferencePrior.h>
#include <lal/LALInferenceTemplate.h>
#include <lal/LALInferenceProposal.h>
#include <lal/LALInferenceLikelihood.h>
#include <lal/LALInferenceReadData.h>
#include <lal/LALInferenceInit.h>
#include <lalapps.h>

#include <mpi.h>


int MPIrank, MPIsize;

static INT4 readSquareMatrix(gsl_matrix *m, UINT4 N, FILE *inp) {
  UINT4 i, j;
  
  for (i = 0; i < N; i++) {
    for (j = 0; j < N; j++) {
      REAL8 value;
      INT4 nread;
      
      nread = fscanf(inp, " %lg ", &value);
      
      if (nread != 1) {
	fprintf(stderr, "Cannot read from matrix file (in %s, line %d)\n",
		__FILE__, __LINE__);
	exit(1);
      }
      
      gsl_matrix_set(m, i, j, value);
    }
  }
  
  return 0;
}


LALInferenceRunState *initialize(ProcessParamsTable *commandLine);
void initializeMCMC(LALInferenceRunState *runState);


/* This contains code chopped from LALInferenceInitCBC that wasn't
 * related to the template intialisation but to the guts of the MCMC
 * algorithm */
void LALInferenceInitMCMCState(LALInferenceRunState *state);
void LALInferenceInitMCMCState(LALInferenceRunState *state)
{
  
  if(state==NULL)
  {
    return;
  }
  LALInferenceVariables *currentParams=state->currentParams;
  LALInferenceVariables *priorArgs=state->priorArgs;
  ProcessParamsTable *commandLine=state->commandLine;
  ProcessParamsTable *ppt=NULL;
  UINT4 i=0;
  
  /* Initialize variable that will store the name of the last proposal function used */
  const char *initPropName = "INITNAME";
  LALInferenceAddVariable(state->proposalArgs, LALInferenceCurrentProposalName, &initPropName, LALINFERENCE_string_t, LALINFERENCE_PARAM_LINEAR);
  
  /* If the currentParams are not in the prior, overwrite and pick paramaters from the priors. OVERWRITE EVEN USER CHOICES.
   *     (necessary for complicated prior shapes where LALInferenceCyclicReflectiveBound() is not enough */
  while(state->prior(state, currentParams)<=-DBL_MAX){
    fprintf(stderr, "Warning initial parameter randlomy drawn from prior. (in %s, line %d)\n",__FILE__, __LINE__);
    LALInferenceVariables *temp; //
    temp=XLALCalloc(1,sizeof(LALInferenceVariables));
    memset(temp,0,sizeof(LALInferenceVariables));
    LALInferenceDrawApproxPrior(state, temp);
    LALInferenceCopyVariables(temp, currentParams);
  }
  /* Make sure that our initial value is within the
   *     prior-supported volume. */
  LALInferenceCyclicReflectiveBound(currentParams, priorArgs);
  
  /* Init covariance matrix, if specified.  The given file
   *     should contain the desired covariance matrix for the jump
   *     proposal, in row-major (i.e. C) order. */
  ppt=LALInferenceGetProcParamVal(commandLine, "--covarianceMatrix");
  if (ppt) {
    FILE *inp = fopen(ppt->value, "r");
    UINT4 N = LALInferenceGetVariableDimensionNonFixed(currentParams);
    gsl_matrix *covM = gsl_matrix_alloc(N,N);
    gsl_matrix *covCopy = gsl_matrix_alloc(N,N);
    REAL8Vector *sigmaVec = XLALCreateREAL8Vector(N);
    
    
    if (readSquareMatrix(covM, N, inp)) {
      fprintf(stderr, "Error reading covariance matrix (in %s, line %d)\n",
	      __FILE__, __LINE__);
      exit(1);
    }
    
    gsl_matrix_memcpy(covCopy, covM);
    
    for (i = 0; i < N; i++) {
      sigmaVec->data[i] = sqrt(gsl_matrix_get(covM, i, i)); /* Single-parameter sigma. */
    }
    
    /* Set up eigenvectors and eigenvalues. */
    gsl_matrix *eVectors = gsl_matrix_alloc(N,N);
    gsl_vector *eValues = gsl_vector_alloc(N);
    REAL8Vector *eigenValues = XLALCreateREAL8Vector(N);
    gsl_eigen_symmv_workspace *ws = gsl_eigen_symmv_alloc(N);
    int gsl_status;
    
    if ((gsl_status = gsl_eigen_symmv(covCopy, eValues, eVectors, ws)) != GSL_SUCCESS) {
      fprintf(stderr, "Error in gsl_eigen_symmv (in %s, line %d): %d: %s\n",
	      __FILE__, __LINE__, gsl_status, gsl_strerror(gsl_status));
      exit(1);
    }
    
    for (i = 0; i < N; i++) {
      eigenValues->data[i] = gsl_vector_get(eValues,i);
    }
    
    LALInferenceAddVariable(state->proposalArgs, "covarianceEigenvectors", &eVectors, LALINFERENCE_gslMatrix_t, LALINFERENCE_PARAM_FIXED);
    LALInferenceAddVariable(state->proposalArgs, "covarianceEigenvalues", &eigenValues, LALINFERENCE_REAL8Vector_t, LALINFERENCE_PARAM_FIXED);
    
    fprintf(stdout, "Jumping with correlated jumps in %d dimensions from file %s.\n",
	    N, ppt->value);
    
    fclose(inp);
    gsl_eigen_symmv_free(ws);
    gsl_matrix_free(covCopy);
    gsl_vector_free(eValues);
  }
  
  /* Differential Evolution? */
  ppt=LALInferenceGetProcParamVal(commandLine, "--noDifferentialEvolution");
  if (!ppt) {
    fprintf(stderr, "Using differential evolution.\nEvery Nskip parameters will be stored for use in the d.e. jump proposal.\n");
    
    state->differentialPoints = XLALCalloc(1, sizeof(LALInferenceVariables *));
    state->differentialPointsLength = 0;
    state->differentialPointsSize = 1;
  } else {
    fprintf(stderr, "Differential evolution disabled (--noDifferentialEvolution).\n");
    state->differentialPoints = NULL;
    state->differentialPointsLength = 0;
    state->differentialPointsSize = 0;
  }
  
  /* kD Tree NCell parameter. */
  ppt=LALInferenceGetProcParamVal(commandLine, "--kDNCell");
  if (ppt) {
    INT4 NCell = atoi(ppt->value);
    LALInferenceAddVariable(state->proposalArgs, "KDNCell", &NCell, LALINFERENCE_INT4_t, LALINFERENCE_PARAM_FIXED);
  }

  /* KD Tree propsal. */
  ppt=LALInferenceGetProcParamVal(commandLine, "--kDTree");
  if (!ppt) {
    ppt = LALInferenceGetProcParamVal(commandLine, "--kdtree");
  }
  if (ppt) {
    LALInferenceKDTree *tree;
    REAL8 *low, *high;
    currentParams = state->currentParams;
    LALInferenceVariables *template = XLALCalloc(1,sizeof(LALInferenceVariables));
    size_t ndim = LALInferenceGetVariableDimensionNonFixed(currentParams);
    LALInferenceVariableItem *currentItem;
    
    low = XLALMalloc(ndim*sizeof(REAL8));
    high = XLALMalloc(ndim*sizeof(REAL8));
    
    currentItem = currentParams->head;
    i = 0;
    while (currentItem != NULL) {
      if (currentItem->vary != LALINFERENCE_PARAM_FIXED) {
	LALInferenceGetMinMaxPrior(state->priorArgs, currentItem->name, &(low[i]), &(high[i]));
	i++;
      }
      currentItem = currentItem->next;
    }
    
    tree = LALInferenceKDEmpty(low, high, ndim);
    LALInferenceCopyVariables(currentParams, template);
    
    LALInferenceAddVariable(state->proposalArgs, "kDTree", &tree, LALINFERENCE_void_ptr_t, LALINFERENCE_PARAM_FIXED);
    LALInferenceAddVariable(state->proposalArgs, "kDTreeVariableTemplate", &template, LALINFERENCE_void_ptr_t, LALINFERENCE_PARAM_FIXED);
  }
  
  INT4 Neff = 0;
  ppt = LALInferenceGetProcParamVal(commandLine, "--Neff");
  if (ppt)
    Neff = atoi(ppt->value);
  LALInferenceAddVariable(state->algorithmParams, "Neff", &Neff, LALINFERENCE_UINT4_t, LALINFERENCE_PARAM_OUTPUT);
  
  return;
}



LALInferenceRunState *initialize(ProcessParamsTable *commandLine)
/* calls the "ReadData()" function to gather data & PSD from files, */
/* and initializes other variables accordingly.                     */
{
  LALInferenceRunState *irs=NULL;
  LALInferenceIFOData *ifoPtr, *ifoListStart;

  MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);

  irs = calloc(1, sizeof(LALInferenceRunState));
  /* read data from files: */
  fprintf(stdout, " ==== LALInferenceReadData(): started. ====\n");
  irs->commandLine=commandLine;
  irs->data = LALInferenceReadData(commandLine);
  /* (this will already initialise each LALInferenceIFOData's following elements:  */
  /*     fLow, fHigh, detector, timeToFreqFFTPlan, freqToTimeFFTPlan,     */
  /*     window, oneSidedNoisePowerSpectrum, timeDate, freqData         ) */
  fprintf(stdout, " ==== LALInferenceReadData(): finished. ====\n");
  if (irs->data != NULL) {
    fprintf(stdout, " ==== initialize(): successfully read data. ====\n");

    fprintf(stdout, " ==== LALInferenceInjectInspiralSignal(): started. ====\n");
    LALInferenceInjectInspiralSignal(irs->data,commandLine);
    fprintf(stdout, " ==== LALInferenceInjectInspiralSignal(): finished. ====\n");

    ifoPtr = irs->data;
    ifoListStart = irs->data;
    while (ifoPtr != NULL) {
      /*If two IFOs have the same sampling rate, they should have the same timeModelh*,
        freqModelh*, and modelParams variables to avoid excess computation
        in model waveform generation in the future*/
      LALInferenceIFOData * ifoPtrCompare=ifoListStart;
      int foundIFOwithSameSampleRate=0;
      while (ifoPtrCompare != NULL && ifoPtrCompare!=ifoPtr) {
        if(ifoPtrCompare->timeData->deltaT == ifoPtr->timeData->deltaT){
          ifoPtr->timeModelhPlus=ifoPtrCompare->timeModelhPlus;
          ifoPtr->freqModelhPlus=ifoPtrCompare->freqModelhPlus;
          ifoPtr->timeModelhCross=ifoPtrCompare->timeModelhCross;
          ifoPtr->freqModelhCross=ifoPtrCompare->freqModelhCross;
          ifoPtr->modelParams=ifoPtrCompare->modelParams;
          foundIFOwithSameSampleRate=1;
          break;
        }
        ifoPtrCompare = ifoPtrCompare->next;
      }
      if(!foundIFOwithSameSampleRate){
        ifoPtr->timeModelhPlus  = XLALCreateREAL8TimeSeries("timeModelhPlus",
                                                            &(ifoPtr->timeData->epoch),
                                                            0.0,
                                                            ifoPtr->timeData->deltaT,
                                                            &lalDimensionlessUnit,
                                                            ifoPtr->timeData->data->length);
        ifoPtr->timeModelhCross = XLALCreateREAL8TimeSeries("timeModelhCross",
                                                            &(ifoPtr->timeData->epoch),
                                                            0.0,
                                                            ifoPtr->timeData->deltaT,
                                                            &lalDimensionlessUnit,
                                                            ifoPtr->timeData->data->length);
        ifoPtr->freqModelhPlus = XLALCreateCOMPLEX16FrequencySeries("freqModelhPlus",
                                                                    &(ifoPtr->freqData->epoch),
                                                                    0.0,
                                                                    ifoPtr->freqData->deltaF,
                                                                    &lalDimensionlessUnit,
                                                                    ifoPtr->freqData->data->length);
        ifoPtr->freqModelhCross = XLALCreateCOMPLEX16FrequencySeries("freqModelhCross",
                                                                     &(ifoPtr->freqData->epoch),
                                                                     0.0,
                                                                     ifoPtr->freqData->deltaF,
                                                                     &lalDimensionlessUnit,
                                                                     ifoPtr->freqData->data->length);
        ifoPtr->modelParams = calloc(1, sizeof(LALInferenceVariables));
      }
      ifoPtr = ifoPtr->next;
    }
    irs->currentLikelihood=LALInferenceNullLogLikelihood(irs->data);
    printf("Injection Null Log Likelihood: %g\n", irs->currentLikelihood);
  }
  else{
    fprintf(stdout, " initialize(): no data read.\n");
    irs = NULL;
    return(irs);
  }

  return(irs);
}

/********** Initialise MCMC structures *********/

/************************************************/
void initializeMCMC(LALInferenceRunState *runState)
{
  char help[]="\
               ---------------------------------------------------------------------------------------------------\n\
               --- General Algorithm Parameters ------------------------------------------------------------------\n\
               ---------------------------------------------------------------------------------------------------\n\
               (--Niter N)                      Number of iterations (2*10^7).\n\
               (--Neff N)                       Number of effective samples. (ends if chain surpasses Niter)\n\
               (--Nskip N)                      Number of iterations between disk save (100).\n\
               (--trigSNR SNR)                  Network SNR from trigger, used to calculate tempMax (injection SNR).\n\
               (--randomseed seed)              Random seed of sampling distribution (random).\n\
               (--adaptTau)                     Adaptation decay power, results in adapt length of 10^tau (5).\n\
               (--noAdapt)                      Do not adapt run.\n\
               \n\
               ---------------------------------------------------------------------------------------------------\n\
               --- Likelihood Functions --------------------------------------------------------------------------\n\
               ---------------------------------------------------------------------------------------------------\n\
               (--zeroLogLike)                  Use flat, null likelihood.\n\
               (--studentTLikelihood)           Use the Student-T Likelihood that marginalizes over noise.\n\
               (--correlatedGaussianLikelihood) Use analytic, correlated Gaussian for Likelihood.\n\
               (--bimodalGaussianLikelihood)    Use analytic, bimodal correlated Gaussian for Likelihood.\n\
               (--rosenbrockLikelihood)         Use analytic, Rosenbrock banana for Likelihood.\n\
               (--analyticnullprior)            Use analytic null prior.\n\
               (--nullprior)                    Use null prior in the sampled parameters.\n\
               (--noiseonly)                    Use signal-free log likelihood (noise model only).\n\
               \n\
               ---------------------------------------------------------------------------------------------------\n\
               --- Noise Model -----------------------------------------------------------------------------------\n\
               ---------------------------------------------------------------------------------------------------\n\
               (--psdFit)                       Run with PSD fitting\n\
               (--psdNblock)                    Number of noise parameters per IFO channel (8)\n\
               (--psdFlatPrior)                 Use flat prior on psd parameters (Gaussian)\n\
               (--removeLines)                  Do include persistent PSD lines in fourier-domain integration\n\
               \n\
               ---------------------------------------------------------------------------------------------------\n\
               --- Proposals  ------------------------------------------------------------------------------------\n\
               ---------------------------------------------------------------------------------------------------\n\
               (--rapidSkyLoc)                  Use rapid sky localization jump proposals.\n\
               (--kDTree)                       Use a kDTree proposal.\n\
               (--kDNCell N)                    Number of points per kD cell in proposal.\n\
               (--covarianceMatrix file)        Find the Cholesky decomposition of the covariance matrix for jumps in file.\n\
               (--proposalSkyRing)              Rotate sky position around vector connecting any two IFOs in network.\n\
               (--proposalCorrPsiPhi)           Jump along psi-phi correlation\n\
               \n\
               ---------------------------------------------------------------------------------------------------\n\
               --- Parallel Tempering Algorithm Parameters -------------------------------------------------------\n\
               ---------------------------------------------------------------------------------------------------\n\
               (--inverseLadder)                Space temperature uniform in 1/T, rather than geometric.\n\
               (--tempSkip N)                   Number of iterations between temperature swap proposals (100).\n\
               (--tempKill N)                   Iteration number to stop temperature swapping (Niter).\n\
               (--tempMin T)                    Lowest temperature for parallel tempering (1.0).\n\
               (--tempMax T)                    Highest temperature for parallel tempering (50.0).\n\
               (--anneal)                       Anneal hot temperature linearly to T=1.0.\n\
               (--annealStart N)                Iteration number to start annealing (5*10^5).\n\
               (--annealLength N)               Number of iterations to anneal all chains to T=1.0 (1*10^5).\n\
               \n\
               ---------------------------------------------------------------------------------------------------\n\
               --- Output ----------------------------------------------------------------------------------------\n\
               ---------------------------------------------------------------------------------------------------\n\
               (--data-dump)                    Output waveforms to file.\n\
               (--adaptVerbose)                 Output parameter jump sizes and acceptance rate stats to file.\n\
               (--tempVerbose)                  Output temperature swapping stats to file.\n\
               (--propVerbose)                  Output proposal stats to file.\n\
               (--outfile file)                 Write output files <file>.<chain_number> (PTMCMC.output.<random_seed>.<chain_number>).\n";

  /* Print command line arguments if runState was not allocated */
  if(runState==NULL)
    {
      fprintf(stdout,"%s",help);
      return;
    }

  INT4 verbose=0,tmpi=0;
  unsigned int randomseed=0;
  REAL8 trigSNR = 0.0;
  REAL8 tempMin = 1.0;
  REAL8 tempMax = 50.0;
  ProcessParamsTable *commandLine=runState->commandLine;
  ProcessParamsTable *ppt=NULL;
  FILE *devrandom;
  struct timeval tv;

  /* Print command line arguments if help requested */
  if(LALInferenceGetProcParamVal(runState->commandLine,"--help"))
    {
      fprintf(stdout,"%s",help);
      runState->algorithm=&PTMCMCAlgorithm;
      return;
    }

  /* Initialise parameters structure */
  runState->algorithmParams=XLALCalloc(1,sizeof(LALInferenceVariables));
  runState->priorArgs=XLALCalloc(1,sizeof(LALInferenceVariables));
  runState->proposalArgs=XLALCalloc(1,sizeof(LALInferenceVariables));
  if(LALInferenceGetProcParamVal(commandLine,"--propVerbose"))
    runState->proposalStats=XLALCalloc(1,sizeof(LALInferenceVariables));

  /* Set up the appropriate functions for the MCMC algorithm */
  runState->algorithm=&PTMCMCAlgorithm;
  runState->evolve=PTMCMCOneStep;

  ppt=LALInferenceGetProcParamVal(commandLine,"--rapidSkyLoc");
  if(ppt)
    runState->proposal=&LALInferenceRapidSkyLocProposal;
  else
    runState->proposal=&LALInferenceDefaultProposal;
    //runState->proposal=&LALInferencetempProposal;

  /* Choose the template generator for inspiral signals */
  LALInferenceInitCBCTemplate(runState);
    
 /* runState->template=&LALInferenceTemplateLAL;
  if(LALInferenceGetProcParamVal(commandLine,"--LALSimulation")){
    runState->template=&LALInferenceTemplateXLALSimInspiralChooseWaveform;
    fprintf(stdout,"Template function called is \"LALInferenceTemplateXLALSimInspiralChooseWaveform\"\n");
  }else{
    ppt=LALInferenceGetProcParamVal(commandLine,"--approximant");
    if(ppt){
      if(strstr(ppt->value,"TaylorF2") || strstr(ppt->value,"TaylorF2RedSpin")) {
        runState->template=&LALInferenceTemplateLAL;
        fprintf(stdout,"Template function called is \"LALInferenceTemplateLAL\"\n");
      }else if(strstr(ppt->value,"35phase_25amp")) {
        runState->template=&LALInferenceTemplate3525TD;
        fprintf(stdout,"Template function called is \"LALInferenceTemplate3525TD\"\n");
      }else{
        runState->template=&LALInferenceTemplateLALGenerateInspiral;
        fprintf(stdout,"Template function called is \"LALInferenceTemplateLALGenerateInspiral\"\n");
      }
    }
  }*/

//  if (LALInferenceGetProcParamVal(commandLine,"--tdlike")) {
//    fprintf(stderr, "Computing likelihood in the time domain.\n");
//    runState->likelihood=&LALInferenceTimeDomainLogLikelihood;
//  } else
  if (LALInferenceGetProcParamVal(commandLine, "--zeroLogLike")) {
    /* Use zero log(L) */
    runState->likelihood=&LALInferenceZeroLogLikelihood;
  } else if (LALInferenceGetProcParamVal(commandLine, "--correlatedGaussianLikelihood")) {
    runState->likelihood=&LALInferenceCorrelatedAnalyticLogLikelihood;
  } else if (LALInferenceGetProcParamVal(commandLine, "--bimodalGaussianLikelihood")) {
    runState->likelihood=&LALInferenceBimodalCorrelatedAnalyticLogLikelihood;
  } else if (LALInferenceGetProcParamVal(commandLine, "--rosenbrockLikelihood")) {
    runState->likelihood=&LALInferenceRosenbrockLogLikelihood;
  } else if (LALInferenceGetProcParamVal(commandLine, "--studentTLikelihood")) {
    fprintf(stderr, "Using Student's T Likelihood.\n");
    runState->likelihood=&LALInferenceFreqDomainStudentTLogLikelihood;
  } else if (LALInferenceGetProcParamVal(commandLine, "--noiseonly")) {
    fprintf(stderr, "Using noise-only likelihood.\n");
    runState->likelihood=&LALInferenceNoiseOnlyLogLikelihood;
  } else {
    runState->likelihood=&LALInferenceUndecomposedFreqDomainLogLikelihood;
  }

  if(LALInferenceGetProcParamVal(commandLine,"--skyLocPrior")){
    runState->prior=&LALInferenceInspiralSkyLocPrior;
  } else if (LALInferenceGetProcParamVal(commandLine, "--correlatedGaussianLikelihood") || 
             LALInferenceGetProcParamVal(commandLine, "--bimodalGaussianLikelihood") ||
             LALInferenceGetProcParamVal(commandLine, "--rosenbrockLikelihood") ||
             LALInferenceGetProcParamVal(commandLine, "--analyticnullprior")) {
    runState->prior=&LALInferenceAnalyticNullPrior;
  } else if (LALInferenceGetProcParamVal(commandLine, "--nullprior")) {
    runState->prior=&LALInferenceNullPrior;
  } else if (LALInferenceGetProcParamVal(commandLine, "--noiseonly")) {
    fprintf(stderr, "Using noise-only prior.\n");fflush(stdout);
    runState->prior=&LALInferenceInspiralNoiseOnlyPrior;
  } else {
    runState->prior=&LALInferenceInspiralPriorNormalised;
  }
  //runState->prior=PTUniformGaussianPrior;

  ppt=LALInferenceGetProcParamVal(commandLine,"--verbose");
  if(ppt) {
    verbose=1;
    LALInferenceAddVariable(runState->algorithmParams,"verbose", &verbose , LALINFERENCE_UINT4_t,
                            LALINFERENCE_PARAM_FIXED);
    set_debug_level("ERROR|INFO");
  }
  else set_debug_level("NDEBUG");

  printf("set iteration number.\n");
  /* Number of live points */
  ppt=LALInferenceGetProcParamVal(commandLine,"--Niter");
  if(ppt)
    tmpi=atoi(ppt->value);
  else {
    tmpi=20000000;
  }
  LALInferenceAddVariable(runState->algorithmParams,"Niter",&tmpi, LALINFERENCE_UINT4_t,LALINFERENCE_PARAM_FIXED);

  printf("set iteration number between disk save.\n");
  /* Number of live points */
  ppt=LALInferenceGetProcParamVal(commandLine,"--Nskip");
  if(ppt)
    tmpi=atoi(ppt->value);
  else {
    tmpi=100;
  }
  LALInferenceAddVariable(runState->algorithmParams,"Nskip",&tmpi, LALINFERENCE_UINT4_t,LALINFERENCE_PARAM_FIXED);

 printf("set trigger SNR.\n");
  /* Network SNR of trigger */
  ppt=LALInferenceGetProcParamVal(commandLine,"--trigSNR");
  if(ppt){
    trigSNR=strtod(ppt->value,(char **)NULL);
  }
  LALInferenceAddVariable(runState->algorithmParams,"trigSNR",&trigSNR,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);

  printf("set lowest temperature.\n");
  /* Minimum temperature of the temperature ladder */
  ppt=LALInferenceGetProcParamVal(commandLine,"--tempMin");
  if(ppt){
    tempMin=strtod(ppt->value,(char **)NULL);
  }
  LALInferenceAddVariable(runState->algorithmParams,"tempMin",&tempMin, LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);

 printf("set highest temperature.\n");
  /* Maximum temperature of the temperature ladder */
  ppt=LALInferenceGetProcParamVal(commandLine,"--tempMax");
  if(ppt){
    tempMax=strtod(ppt->value,(char **)NULL);
  }
  LALInferenceAddVariable(runState->algorithmParams,"tempMax",&tempMax, LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);

  printf("set random seed.\n");
  /* set up GSL random number generator: */
  gsl_rng_env_setup();
  runState->GSLrandom = gsl_rng_alloc(gsl_rng_mt19937);
  /* (try to) get random seed from command line: */
  ppt = LALInferenceGetProcParamVal(commandLine, "--randomseed");
  if (ppt != NULL)
    randomseed = atoi(ppt->value);
  else { /* otherwise generate "random" random seed: */
    if ((devrandom = fopen("/dev/urandom","r")) == NULL) {
      if (MPIrank == 0) {
        gettimeofday(&tv, 0);
        randomseed = tv.tv_sec + tv.tv_usec;
      }
      MPI_Bcast(&randomseed, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    }
    else {
      if (MPIrank == 0) {
        fread(&randomseed, sizeof(randomseed), 1, devrandom);
        fclose(devrandom);
      }
      MPI_Bcast(&randomseed, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);
  fprintf(stdout, " initialize(): random seed: %u\n", randomseed);
  LALInferenceAddVariable(runState->algorithmParams,"random_seed",&randomseed, LALINFERENCE_UINT4_t,LALINFERENCE_PARAM_FIXED);
  gsl_rng_set(runState->GSLrandom, randomseed);


  /* Now make sure that everyone is running with un-correlated
     jumps!  We re-seed rank i process with the ith output of
     the RNG stream from the rank 0 process. Otherwise the
     random stream is the same across all processes. */
  INT4 i;
  for (i = 0; i < MPIrank; i++) {
    randomseed = gsl_rng_get(runState->GSLrandom);
  }
  gsl_rng_set(runState->GSLrandom, randomseed);

  /* Differential Evolution? */
  ppt=LALInferenceGetProcParamVal(commandLine, "--noDifferentialEvolution");
  if (!ppt) {
    fprintf(stderr, "Using differential evolution.\nEvery Nskip parameters will be stored for use in the d.e. jump proposal.\n");
    
    runState->differentialPoints = XLALCalloc(1, sizeof(LALInferenceVariables *));
    runState->differentialPointsLength = 0;
    runState->differentialPointsSize = 1;
  } else {
    fprintf(stderr, "Differential evolution disabled (--noDifferentialEvolution).\n");
    runState->differentialPoints = NULL;
    runState->differentialPointsLength = 0;
    runState->differentialPointsSize = 0;
  }
  
  INT4 Neff = 0;
  ppt = LALInferenceGetProcParamVal(commandLine, "--Neff");
  if (ppt)
    Neff = atoi(ppt->value);
  LALInferenceAddVariable(runState->algorithmParams, "Neff", &Neff, LALINFERENCE_UINT4_t, LALINFERENCE_PARAM_OUTPUT);
  
  return;

}


int main(int argc, char *argv[]){
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);
  MPI_Comm_size(MPI_COMM_WORLD, &MPIsize);

  if (MPIrank == 0) fprintf(stdout," ========== LALInference_MCMC ==========\n");

  LALInferenceRunState *runState;
  ProcessParamsTable *procParams=NULL;
  ProcessParamsTable *ppt=NULL;
  char *infileName;
  infileName = (char*)calloc(99,sizeof(char*));
  char str [999];
  FILE * infile;
  int n;
  char * pch;
  int fileargc = 1;
  char *fileargv[99];
  char buffer [99];

  /* Read command line and parse */
  procParams=LALInferenceParseCommandLine(argc,argv);

  ppt=LALInferenceGetProcParamVal(procParams,"--continue-run");
  if (ppt) {
    infileName = ppt->value;
    infile = fopen(infileName,"r");
    if (infile==NULL) {fprintf(stderr,"Cannot read %s/n",infileName); exit (1);}
    n=sprintf(buffer,"lalinference_mcmcmpi_from_file_%s",infileName);
    fileargv[0] = (char*)calloc((n+1),sizeof(char*));
    fileargv[0] = buffer;
    fgets(str, 999, infile);
    fgets(str, 999, infile);
    fclose(infile);
    pch = strtok (str," ");
    while (pch != NULL)
      {
        if(strcmp(pch,"Command")!=0 && strcmp(pch,"line:")!=0)
          {
            n = strlen(pch);
            fileargv[fileargc] = (char*)calloc((n+1),sizeof(char*));
            fileargv[fileargc] = pch;
            fileargc++;
            if(fileargc>=99) {fprintf(stderr,"Too many arguments in file %s\n",infileName); exit (1);}
          }
        pch = strtok (NULL, " ");

      }
    fileargv[fileargc-1][strlen(fileargv[fileargc-1])-1]='\0'; //in order to get rid of the '\n' than fgets returns when reading the command line.

    procParams=LALInferenceParseCommandLine(fileargc,fileargv);
  }


  /* initialise runstate based on command line */
  /* This includes reading in the data */
  /* And performing any injections specified */
  /* And allocating memory */
  runState = initialize(procParams);

  /* Set up structures for MCMC */
  initializeMCMC(runState);
  if (runState)
    LALInferenceAddVariable(runState->algorithmParams,"MPIrank", &MPIrank, LALINFERENCE_UINT4_t,
                          LALINFERENCE_PARAM_FIXED);

  /* Set up currentParams with variables to be used */
  LALInferenceInitCBCVariables(runState);
  
  /* Call the extra code that was removed from previous function */
  LALInferenceInitMCMCState(runState);
  
  if(runState==NULL) {
    fprintf(stderr, "runState not allocated (%s, line %d).\n",
            __FILE__, __LINE__);
    exit(1);
  }
  printf(" ==== This is thread %d of %d ====\n", MPIrank, MPIsize);
  MPI_Barrier(MPI_COMM_WORLD);
  /* Call MCMC algorithm */
  runState->algorithm(runState);

  if (MPIrank == 0) printf(" ========== main(): finished. ==========\n");
  MPI_Finalize();
  return 0;
}
