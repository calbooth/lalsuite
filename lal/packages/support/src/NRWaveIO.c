/*
 * Copyright (C) 2006 S.Fairhurst, B. Krishnan, L.Santamaria
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

/** \file NRWaveIO.c
 *  \ingroup NRWaveIO
 *  \author S.Fairhurst, B.Krishnan, L.Santamaria
 * 
 *  \brief Functions for reading/writing numerical relativity waveforms
 *
 * $Id$ 
 *
 */

#include <lal/LALStdio.h>
#include <lal/FileIO.h>
#include <lal/NRWaveIO.h>
#include <lal/NRWaveInject.h> 

NRCSID( NRWAVEIOC, "$Id$");



/** Reads a numerical relativity waveform given a filename and a value of the 
    total mass for setting the timescale.  The output waveform is scaled corresponding
    to a distance of 1Mpc.
*/
void LALReadNRWave(LALStatus *status, 
		   REAL4TimeVectorSeries **out, /**< [out] output time series for h+ and hx */
		   const REAL4  mass,           /**< [in] Value of total mass for setting time scale */
		   const CHAR  *filename        /**< [in] File containing numrel waveform */) 
{

  UINT4 length, k, r;
  REAL4TimeVectorSeries *ret=NULL;
  REAL4VectorSequence *data=NULL;
  REAL4Vector *timeVec=NULL;
  REAL4 massMpc;
  LALParsedDataFile *cfgdata=NULL;
  REAL4 tmp1, tmp2, tmp3;

  INITSTATUS (status, "LALReadNRWave", NRWAVEIOC);
  ATTATCHSTATUSPTR (status); 
 
  /* some consistency checks */
  ASSERT (filename != NULL, status, NRWAVEIO_ENULL, NRWAVEIO_MSGENULL );
  ASSERT ( mass > 0, status, NRWAVEIO_EVAL, NRWAVEIO_MSGEVAL );
  ASSERT ( out != NULL, status, NRWAVEIO_ENULL, NRWAVEIO_MSGENULL );
  ASSERT ( *out == NULL, status, NRWAVEIO_ENONULL, NRWAVEIO_MSGENONULL );

  TRY( LALParseDataFile ( status->statusPtr, &cfgdata, filename), status);
  length = cfgdata->lines->nTokens; /*number of data points */


  /* allocate memory */
  ret = LALCalloc(1, sizeof(*ret));
  if (!ret) {
    ABORT( status, NRWAVEIO_ENOMEM, NRWAVEIO_MSGENOMEM );
  }
  strcpy(ret->name,filename);
  ret->f0 = 0;
  
  data =  XLALCreateREAL4VectorSequence (2, length);
  if (!data) {
    ABORT( status, NRWAVEIO_ENOMEM, NRWAVEIO_MSGENOMEM );
  }
  
  timeVec = XLALCreateREAL4Vector (length);
  if (!timeVec) {
    ABORT( status, NRWAVEIO_ENOMEM, NRWAVEIO_MSGENOMEM );
  }

  /* mass in Mpc -- we multiply h(t) by this factor */
  massMpc = mass * LAL_MRSUN_SI / ( LAL_PC_SI * 1.0e6);

  /* now get the data */
  for (k = 0; k < length; k++) {
    r = sscanf(cfgdata->lines->tokens[k], "%f%f%f", &tmp1, &tmp2, &tmp3);    

    /* Check the data file format */
    if ( r != 3) {
      /* there must be exactly 3 data entries -- time, h+, hx */
      ABORT( status, NRWAVEIO_EFORMAT, NRWAVEIO_MSGEFORMAT );
    }

    timeVec->data[k] = tmp1;
    data->data[k] = massMpc * tmp2;
    data->data[data->vectorLength + k] = massMpc * tmp3;
     
  }

  /*  scale time */
  ret->deltaT = LAL_MTSUN_SI * mass * ( timeVec->data[1] - timeVec->data[0]);

  /* go through timeVec to make sure it is uniformly spaced */


  ret->data = data;
  (*out) = ret;

  XLALDestroyREAL4Vector (timeVec);
  TRY( LALDestroyParsedDataFile (status->statusPtr, &cfgdata), status);

  DETATCHSTATUSPTR(status);
  RETURN(status);
  
} /* LALReadNRWave() */



/** Function for reading a numerical relativity metadata file.
    It returns a list of numrel wave parameters.  It uses 
    LALParseDataFile() for reading the data file.  This automatically
    takes care of removing comment lines starting with # and other details. 
 */
void
LALNRDataFind( LALStatus *status,
	       NRWaveCatalog *out,  /**< [out] list of numrel metadata */
	       const CHAR *dir,     /**< [in] directory with data files */
	       const CHAR *filename /**< [in] File with metadata information */)
{
  LALParsedDataFile *cfgdata=NULL;
  UINT4 k, numWaves;

  INITSTATUS (status, "LALNRDataFind", NRWAVEIOC);
  ATTATCHSTATUSPTR (status); 

  ASSERT (filename != NULL, status, NRWAVEIO_ENULL, NRWAVEIO_MSGENULL );
  ASSERT ( out != NULL, status, NRWAVEIO_ENULL, NRWAVEIO_MSGENULL );
  ASSERT ( dir != NULL, status, NRWAVEIO_ENULL, NRWAVEIO_MSGENULL );

  TRY( LALParseDataFile ( status->statusPtr, &cfgdata, filename), status);
  numWaves = cfgdata->lines->nTokens; /*number of waves */

  /* allocate memory for output catalog */
  out->length = numWaves;
  out->data = LALCalloc(1, out->length * sizeof(NRWaveMetaData));

  /* now get wave parameters from each line of data */
  for (k = 0; k < numWaves; k++) {
    TRY(LALGetSingleNRMetaData( status->statusPtr, out->data + k, dir, cfgdata->lines->tokens[k]), status);
  }

  TRY( LALDestroyParsedDataFile (status->statusPtr, &cfgdata), status);

  DETATCHSTATUSPTR(status);
  RETURN(status);
}



/** Parse a single string to fill the NRWaveMetaData structure */
void
LALGetSingleNRMetaData( LALStatus       *status,
			NRWaveMetaData  *out,   /**< [out] Meta data struct to be filled */
			const CHAR      *dir,   /**< [in] data directory */
			const CHAR      *cfgstr /**< [in] config string containing the data for a single NR wave*/) 
{
  REAL4 tmpR[7];
  INT4  tmpI[2];
  INT4 test;
  CHAR tmpStr[512];

  INITSTATUS (status, "LALGetSingleNRMetaData", NRWAVEIOC);
  ATTATCHSTATUSPTR (status); 

  ASSERT (cfgstr != NULL, status, NRWAVEIO_ENULL, NRWAVEIO_MSGENULL );
  ASSERT (out != NULL, status, NRWAVEIO_ENULL, NRWAVEIO_MSGENULL );
  ASSERT (dir != NULL, status, NRWAVEIO_ENULL, NRWAVEIO_MSGENULL );

  test = sscanf(cfgstr, "%f%f%f%f%f%f%f%d%d%s", tmpR, tmpR+1, tmpR+2, tmpR+3, tmpR+4, tmpR+5, 
	 tmpR+6, tmpI, tmpI+1, tmpStr);

  /* Check the metadata file format */
  if ( test != 10) {
    /* there must be exactly 10 data entries 
       -- massratio, spin1[3], spin2[3], l, m, filename */
    ABORT( status, NRWAVEIO_EFORMAT, NRWAVEIO_MSGEFORMAT );
  }

  /* the mass ratio must be positive */
  ASSERT (tmpR[0] >= 0, status, NRWAVEIO_EVAL, NRWAVEIO_MSGEVAL );

  /*** need a few more format checks here ***/

  /* copy values to out */
  out->massRatio = tmpR[0];
  out->spin1[0] = tmpR[1];
  out->spin1[1] = tmpR[2];
  out->spin1[2] = tmpR[3];
  out->spin2[0] = tmpR[4];
  out->spin2[1] = tmpR[5];
  out->spin2[2] = tmpR[6];

  out->mode[0] = tmpI[0];
  out->mode[1] = tmpI[1];

  strcpy(out->filename, dir);
  strcat(out->filename, "/");
  strcat(out->filename, tmpStr);

  DETATCHSTATUSPTR(status);
  RETURN(status);
}


/** Put the main functionalities of nr_wave.c together */
void
LALDriveNRWave(
  LALStatus              *status,
  REAL4TimeVectorSeries  **outStrain,       /* h+, hx data                    */
  NRWaveMetaData         *thisMetaData, /* single NR wave metadata struct */
  NRWaveCatalog          *nrCatalog,    /* NR wave metadata struct        */
  INT4                   modeLlo,      /* contains modeLlo and modeLhi   */
  INT4                   modeLhi,      /* modeLhi                        */
  SimInspiralTable       *thisInj     /* injection                      */)
{
  INT4 modeL, modeM;
  REAL4TimeVectorSeries *sumStrain=NULL;
  REAL4TimeVectorSeries *tempStrain=NULL;

  INITSTATUS (status, "LALDriveNRWave", NRWAVEIOC);
  ATTATCHSTATUSPTR (status); 

  /* loop over l values */
  for ( modeL = modeLlo; modeL <= modeLhi; modeL++ )
    {
      /* loop over m values */
      for ( modeM = -modeL; modeM <= modeL; modeM++ )
        {
          /* find nearest matching numrel waveform */
          XLALFindNRFile( &thisMetaData, nrCatalog, thisInj, modeL, modeM );

          /* read numrel waveform */
          TRY( LALReadNRWave( status->statusPtr, &tempStrain, thisInj->mass1 +
                thisInj->mass2, thisMetaData->filename ), status );


	  /*     if ( vrbflg ) */
	  /*           { */
	  /*             fprintf( stdout, "done\n" ); */
	  /*           } */
	  
	  /*           if ( vrbflg ) */
	  /*           { */
	  /*             fprintf( stdout, */
	  /*                 "Generating waveform for inclination = %f, coa_phase = %f\n", */
	  /*                 thisInj->inclination, thisInj->coa_phase ); */
	  /*           } */

          /* compute the h+ and hx for given inclination and coalescence phase*/
          tempStrain = XLALOrientNRWave( tempStrain, thisMetaData->mode[0],
              thisMetaData->mode[1], thisInj->inclination, thisInj->coa_phase );

	  fprintf(stdout, "Elemento de strain= %e\n", tempStrain->data->data[0]);


	  if (sumStrain == NULL) {

	    sumStrain = LALCalloc(1, sizeof(*sumStrain));	    

	    sumStrain->data =  XLALCreateREAL4VectorSequence(2, tempStrain->data->vectorLength); 
	    sumStrain->deltaT = tempStrain->deltaT;
	    sumStrain->f0 = tempStrain->f0; 
	    sumStrain->sampleUnits = tempStrain->sampleUnits; 

	    memset(sumStrain->data->data,0.0,2*tempStrain->data->vectorLength);

	    sumStrain = XLALSumStrain( sumStrain, tempStrain );
	  }

	  else {
	    sumStrain = XLALSumStrain( sumStrain, tempStrain );
	  }

	  fprintf(stdout, "Elemento de sumStrain= %e\n", sumStrain->data->data[0]);

          /* clear memory for strain */
          XLALDestroyREAL4VectorSequence ( tempStrain->data );
          LALFree( tempStrain );
          tempStrain = NULL;

        } /* end loop over modeM values */

      } /* end loop over modeL values */


  *outStrain = sumStrain;
      
  DETATCHSTATUSPTR(status);
  RETURN(status);


/*       /\* Assign length to the temporal variable *\/ */
/*       /\* NOTE: Implies that all modes have the same length!!*\/ */
/*       XLALFindNRFile( &thisMetaData, &nrCatalog, thisInj, 2, -2 ); */
/*       LALReadNRWave( &status, &strain, thisInj->mass1 + */
/* 		     thisInj->mass2, thisMetaData.filename ); */
/*       length = strain->data->vectorLength; */

/*       /\* Inicialize tempstrain as zero *\/ */
/*       tempstrain = LALCalloc(1, sizeof(*tempstrain));   */
/*       data =  XLALCreateREAL4VectorSequence(2, length); */
/*       tempstrain->data = data; */
/*       tempstrain->deltaT = strain->deltaT; */
/*       tempstrain->f0 = strain->f0; */
/*       tempstrain->sampleUnits = strain->sampleUnits; */
/*       for (r=0; r<data->length*data->vectorLength; r++) */
/* 	{ */
/* 	  tempstrain->data->data[r] = 0.0; */
/* 	} */

/*       /\* Reset strain as null pointer *\/ */
/*       XLALDestroyREAL4VectorSequence ( strain->data ); */
/*       LALFree( strain ); */
/*       strain = NULL; */
 
/*       /\* loop over l values *\/ */
/*       for ( modeL = modeLlo; modeL <= modeLhi; modeL++ ) */
/*       { */
/*         /\* loop over m values *\/ */
/*         for ( modeM = -modeL; modeM <= modeL; modeM++ ) */
/*         { */
/*           /\* find nearest matching numrel waveform *\/ */
/*           XLALFindNRFile( &thisMetaData, &nrCatalog, thisInj, modeL, modeM ); */

/*             fprintf( stdout, "Reading the waveform from the file \"%s\"...", */
/*                 thisMetaData.filename ); */

/*           /\* read numrel waveform *\/ */
/*           LALReadNRWave( &status, &strain, thisInj->mass1 + */
/*                 thisInj->mass2, thisMetaData.filename ); */

/*             fprintf( stdout, "done\n" ); */

/*             fprintf( stdout, */
/*                 "Generating waveform for inclination = %f, coa_phase = %f\n", */
/*                 thisInj->inclination, thisInj->coa_phase ); */

/*           /\* compute the h+ and hx for given inclination and coalescence phase*\/ */
/*           strain = XLALOrientNRWave( strain, thisMetaData.mode[0], */
/*               thisMetaData.mode[1], thisInj->inclination, thisInj->coa_phase ); */

/* 	  fprintf(stdout, "Elemento de strain= %e\n", strain->data->data[0]); */

/* 	  tempstrain = XLALSumStrain( tempstrain, strain ); */

/* 	  fprintf(stdout, "Elemento de tempstrain= %e\n", tempstrain->data->data[0]); */

/*           /\* clear memory for strain *\/ */
/*           XLALDestroyREAL4VectorSequence ( strain->data ); */
/*           LALFree( strain ); */
/*           strain = NULL; */

/*         } /\* end loop over modeM values *\/ */

/*       } /\* end loop over modeL values *\/ */

/*       /\* copy tempstrain into strain *\/ */
/*       strain = LALCalloc(1, sizeof(*strain));   */
/*       data =  XLALCreateREAL4VectorSequence(2, length); */
/*       strain->data = data; */
/*       strain->deltaT = tempstrain->deltaT; */
/*       strain->f0 = tempstrain->f0; */
/*       strain->sampleUnits = tempstrain->sampleUnits; */
/*       for (r=0; r<data->length*data->vectorLength; r++) */
/* 	{ */
/* 	  strain->data->data[r] = tempstrain->data->data[r]; */
/* 	} */
	  
/*           file = fopen("tempstrain.dat","w"); */

/* 	  r = 0; */
/* 	  for ( r =0; r < tempstrain->data->vectorLength; r++) */
/* 	    { */
/* 	      fprintf(file,"%d %e %e \n", r, tempstrain->data->data[r], tempstrain->data->data[strain->data->vectorLength + r]); */
/* 	    }; */

/* 	  fclose(file);    */

/*       fprintf( stdout, "Test 15th May\n"); */


}
