/**
 ** \file DynamicSpikingSynapse_struct.cpp
 **
 ** \authors oldman
 ** \brief A dynamic spiking synapse (Makram et al (1998))
 **/

#include "DynamicSpikingSynapse_struct.h"
#include <string.h>

/**
 * Allocate data members in the DynamicSpikingSynapse_struct.
 * @param synapse
 * @param count
 */
void allocSynapseStruct(DynamicSpikingSynapse_struct& synapse, int count) {
	synapse.inUse		= new bool[count];
	memset(synapse.inUse, 0, count * sizeof(bool));
	synapse.summationPoint 	= new PFLOAT[count];
	synapse.summationCoord 	= new Coordinate[count];
	synapse.synapseCoord 	= new Coordinate[count];
	synapse.deltaT 		= new FLOAT[count];
	synapse.W 		= new FLOAT[count];
	synapse.psr 		= new FLOAT[count];
	synapse.decay 		= new FLOAT[count];
	synapse.total_delay 	= new int[count];
	synapse.type 		= new synapseType[count];
	synapse.delayQueue 	= new uint32_t[count];
	synapse.ldelayQueue 	= new int[count];
	synapse.tau 		= new FLOAT[count];
	synapse.r 		= new FLOAT[count];
	synapse.u 		= new FLOAT[count];
	synapse.lastSpike 	= new uint64_t[count];	
}

/**
 * Deallocate data members in the DynamicSpikingSynapse_struct.
 * @param synapse
 */
void deleteSynapseStruct(DynamicSpikingSynapse_struct& synapse) {
	delete[] synapse.inUse;
	delete[] synapse.summationPoint;
	delete[] synapse.summationCoord;
	delete[] synapse.synapseCoord;
	delete[] synapse.deltaT;
	delete[] synapse.W;
	delete[] synapse.psr;
	delete[] synapse.decay;
	delete[] synapse.total_delay;
	delete[] synapse.type;
	delete[] synapse.delayQueue;
	delete[] synapse.ldelayQueue;
	delete[] synapse.tau;
	delete[] synapse.r;
	delete[] synapse.u;
	delete[] synapse.lastSpike;
}

/**
 * Allocate data members (inUse and summationCoord) in the DynamicSpikingSynapse_struct.
 * @param synapse
 * @param count
 */
void allocSynapseSumCoord(DynamicSpikingSynapse_struct& synapse, int count) {
	synapse.inUse           = new bool[count];
	memset(synapse.inUse, 0, count * sizeof(bool));
	synapse.summationCoord  = new Coordinate[count];
}

/**
 * Deallocate data members (inUse and summationCoord) in the DynamicSpikingSynapse_struct.
 * @param synapse
 */
void deleteSynapseSumCoord(DynamicSpikingSynapse_struct& synapse) {
	delete[] synapse.inUse;
	delete[] synapse.summationCoord;
}

/**
 * Copy a DynamicSpikingSynapse into a DynamicSpikingSynapse_struct.
 * @param in
 * @param out
 * @param idx
 */
void copySynapseToStruct(DynamicSpikingSynapse& in, DynamicSpikingSynapse_struct& out, int idx) {
	// copy everything necessary
	out.inUse[idx] 			= true;
	out.W[idx] 			= in.W;
	out.decay[idx] 			= in.decay;
	out.deltaT[idx] 		= in.deltaT;
	out.lastSpike[idx] 		= in.lastSpike;
	out.psr[idx] 			= in.psr;
	out.r[idx] 			= in.r;
	out.summationCoord[idx] 	= in.summationCoord;
	out.synapseCoord[idx] 		= in.synapseCoord;
	out.summationPoint[idx] 	= 0;
	out.tau[idx] 			= in.tau;
	out.total_delay[idx] 		= in.total_delay;
	out.u[idx] 			= in.u;
	out.ldelayQueue[idx] 		= in.ldelayQueue;
	out.type[idx] 			= in.type;
	out.delayQueue[idx] 		= in.delayQueue[0];
}

/**
 * Copy a DynamicSpikingSynapse_struct into a DynamicSpikingSynapse.
 * @param in
 * @param out
 * @param idx
 */
void copyStructToSynapse(DynamicSpikingSynapse_struct& in, DynamicSpikingSynapse& out, int idx) {
	// copy everything necessary
	out.W 			= in.W[idx];
	out.decay 		= in.decay[idx];
	out.deltaT 		= in.deltaT[idx];
	out.lastSpike 		= in.lastSpike[idx];
	out.psr 		= in.psr[idx];
	out.r 			= in.r[idx];
	out.summationCoord 	= in.summationCoord[idx];
	out.synapseCoord	= in.synapseCoord[idx];
	out.tau 		= in.tau[idx];
	out.total_delay 	= in.total_delay[idx];
	out.u 			= in.u[idx];
	out.type		= in.type[idx];
	out.delayQueue[0] 	= in.delayQueue[idx];
}

/**
 * Copy a synapseArray into a synapseMap.
 * @param synapse
 * @param synapseMap
 * @param numNeurons
 */
void synapseArrayToMap(DynamicSpikingSynapse_struct& synapse_st, vector<DynamicSpikingSynapse> * synapseMap, int numNeurons, int maxSynapses)
{
	// craete a synapse
	FLOAT sp;
	DynamicSpikingSynapse syn(0, 0, 0, 0, sp, DEFAULT_delay_weight, 0.0001, II);

	for (int neuron_i = 0; neuron_i < numNeurons; neuron_i++)
	{
		synapseMap[neuron_i].clear();
		for (int j = 0; j < maxSynapses; j++)
		{
			if (synapse_st.inUse[neuron_i * maxSynapses + j] == true)
			{
				copyStructToSynapse( synapse_st, syn, neuron_i * maxSynapses + j );
				synapseMap[neuron_i].push_back(syn);
			}
		}
	}
}
