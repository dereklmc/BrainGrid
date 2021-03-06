/**
 *	@file Network.cpp
 *
 *	@author Allan Ortiz & Cory Mayberry
 *
 *  @brief A grid of LIF Neurons and their interconnecting synapses.
 */
#include "Network.h"

/** 
 * The constructor for Network.
 * @post The network is setup according to parameters and ready for simulation.
 */
Network::Network(int cols, int rows, FLOAT inhFrac, FLOAT excFrac, FLOAT startFrac, FLOAT Iinject[2],
        FLOAT Inoise[2], FLOAT Vthresh[2], FLOAT Vresting[2], FLOAT Vreset[2], FLOAT Vinit[2],
        FLOAT starter_Vthresh[2], FLOAT starter_Vreset[2], FLOAT new_epsilon, FLOAT new_beta, FLOAT new_rho,
        FLOAT new_targetRate, FLOAT new_maxRate, FLOAT new_minRadius, FLOAT new_startRadius, FLOAT new_deltaT,
        ostream& new_stateout, ostream& new_memoutput, bool fWriteMemImage, istream& new_meminput, bool fReadMemImage, 
	bool fFixedLayout, vector<int>* pEndogenouslyActiveNeuronLayout, vector<int>* pInhibitoryNeuronLayout) :
    m_width(cols),
    m_height(rows),
    m_cNeurons(cols * rows),
    m_cExcitoryNeurons(static_cast<int>(rows * cols * excFrac)), 
    m_cInhibitoryNeurons(static_cast<int>(rows * cols* inhFrac)), 
    m_cStarterNeurons(static_cast<int>(m_width * m_height * startFrac)), 
    m_deltaT(new_deltaT),
    m_rgSynapseMap(NULL),
    m_summationMap(NULL),
    m_rgNeuronTypeMap(NULL),
    m_rgEndogenouslyActiveNeuronMap(NULL),
    m_epsilon(new_epsilon), 
    m_beta(new_beta), 
    m_rho(new_rho),
    m_targetRate(new_targetRate),
    m_maxRate(new_maxRate),
    m_minRadius(new_minRadius),
    m_startRadius(new_startRadius),
    state_out(new_stateout),
    memory_out(new_memoutput),
    m_fWriteMemImage(fWriteMemImage),
    memory_in(new_meminput),
    m_fReadMemImage(fReadMemImage),
    m_fFixedLayout(fFixedLayout),
    m_pEndogenouslyActiveNeuronLayout(pEndogenouslyActiveNeuronLayout),
    m_pInhibitoryNeuronLayout(pInhibitoryNeuronLayout)
{
    cout << "Neuron count: " << m_cNeurons << endl;
 
    // init data structures
    reset();

    // init neurons
    initNeurons(Iinject, Inoise, Vthresh, Vresting, Vreset, Vinit, starter_Vthresh, starter_Vreset);
}

/**
* Destructor
*
*/
Network::~Network()
{
    freeResources();
}

/**
* Run simulation
*
* @param growthStepDuration
* @param maxGrowthSteps
*/
void Network::simulate(FLOAT growthStepDuration, FLOAT maxGrowthSteps, int maxFiringRate, int maxSynapsesPerNeuron)
{
    ISimulation* pSim = NULL;
    string matrixType, init;

    matrixType = "complete";
    init = "const";
    VectorMatrix radii(matrixType, init, 1, m_cNeurons);	// previous saved radii
    VectorMatrix rates(matrixType, init, 1, m_cNeurons);	// previous saved rates

    // Init SimulationInfo parameters
    m_si.stepDuration = growthStepDuration;
    m_si.maxSteps = (int)maxGrowthSteps;
    m_si.maxFiringRate = maxFiringRate;
    m_si.maxSynapsesPerNeuron = maxSynapsesPerNeuron;
    m_si.width = m_width;
    m_si.height = m_height;
    m_si.rgNeuronTypeMap = m_rgNeuronTypeMap;
    m_si.rgEndogenouslyActiveNeuronMap = m_rgEndogenouslyActiveNeuronMap;
    m_si.epsilon = m_epsilon;
    m_si.beta = m_beta;
    m_si.rho = m_rho;
    m_si.maxRate = m_maxRate;
    m_si.minRadius = m_minRadius;
    m_si.startRadius = m_startRadius;

    // burstiness Histogram goes through the
    VectorMatrix burstinessHist(matrixType, init, 1, (int)(growthStepDuration * maxGrowthSteps), 0);

    // spikes history - history of accumulated spikes count of all neurons (10 ms bin)
    VectorMatrix spikesHistory(matrixType, init, 1, (int)(growthStepDuration * maxGrowthSteps * 100), 0);

    // track radii
    CompleteMatrix radiiHistory(matrixType, init, static_cast<int>(maxGrowthSteps + 1), m_cNeurons);

    // track firing rate
    CompleteMatrix ratesHistory(matrixType, init, static_cast<int>(maxGrowthSteps + 1), m_cNeurons);

    // neuron types
    VectorMatrix neuronTypes(matrixType, init, 1, m_cNeurons, EXC);

    // neuron threshold
    VectorMatrix neuronThresh(matrixType, init, 1, m_cNeurons, 0);
    for (int i = 0; i < m_cNeurons; i++) 
    {
        neuronThresh[i] = m_neuronList[i].Vthresh;
    }

    // neuron locations matrices
    VectorMatrix xloc(matrixType, init, 1, m_cNeurons);
    VectorMatrix yloc(matrixType, init, 1, m_cNeurons);

    // Initialize neurons
    for (int i = 0; i < m_cNeurons; i++)
    {
        xloc[i] = i % m_width;
        yloc[i] = i / m_width;
    }

    // Populate neuron types with current values
    getNeuronTypes(neuronTypes);

    // Init radii and rates history matrices with current radii and rates
    for (int i = 0; i < m_cNeurons; i++)
    {
        radiiHistory(0, i) = m_startRadius;
        ratesHistory(0, i) = 0;
    }

    // Read a simulation memory image
    if (m_fReadMemImage)
    {
        readSimMemory(memory_in, radii, rates);
        for (int i = 0; i < m_cNeurons; i++)
        {
            radiiHistory(0, i) = radii[i];
            ratesHistory(0, i) = rates[i];
        }
    }

    // Start the timer
    // TODO: stop the timer at some point and use its output
    m_timer.start();

    // Get an ISimulation object
    // TODO: remove #defines and use cmdline parameters to choose simulation method
#if defined(USE_GPU)
    pSim = new GpuSim(&m_si);
#elif defined(USE_OMP)
    pSim = new MultiThreadedSim(&m_si);

    // Initialize OpenMP - one thread per core
    OMP(omp_set_num_threads(omp_get_num_procs());)

    int max_threads = 1;
    max_threads = omp_get_max_threads();

    // Create normalized random number generators for each thread
    for (int i = 0; i < max_threads; i++)
        rgNormrnd.push_back(new Norm(0, 1, 1));
#else
    pSim = new SingleThreadedSim(&m_si);

    // Create a normalized random number generator
    rgNormrnd.push_back(new Norm(0, 1, 1));
#endif

    pSim->init(&m_si, xloc, yloc);

    // Set the previous saved radii
    if (m_fReadMemImage)
    {
        pSim->initRadii(radii);
    }

    // Main simulation loop - execute maxGrowthSteps
    for (int currentStep = 1; currentStep <= maxGrowthSteps; currentStep++)
    {
#ifdef PERFORMANCE_METRICS
        m_timer.start();
#endif

        // Init SimulationInfo parameters
        m_si.currentStep = currentStep;

        DEBUG(cout << "\n\nPerforming simulation number " << currentStep << endl;)
        DEBUG(cout << "Begin network state:" << endl;)

        // Advance simulation to next growth cycle
        pSim->advanceUntilGrowth(&m_si);

        DEBUG(cout << "\n\nDone with simulation cycle, beginning growth update " << currentStep << endl;)

        // Update the neuron network
#ifdef PERFORMANCE_METRICS
        m_short_timer.start();
#endif
	pSim->updateNetwork(&m_si, radiiHistory, ratesHistory);

#ifdef PERFORMANCE_METRICS
        t_host_adjustSynapses = m_short_timer.lap() / 1000.0f;
	float total_time = m_timer.lap() / 1000.0f;
	float t_others = total_time - (t_gpu_rndGeneration + t_gpu_advanceNeurons + t_gpu_advanceSynapses + t_gpu_calcSummation + t_host_adjustSynapses);

        cout << endl;
        cout << "total_time: " << total_time << " ms" << endl;
        cout << "t_gpu_rndGeneration: " << t_gpu_rndGeneration << " ms (" << t_gpu_rndGeneration / total_time * 100 << "%)" << endl;
        cout << "t_gpu_advanceNeurons: " << t_gpu_advanceNeurons << " ms (" << t_gpu_advanceNeurons / total_time * 100 << "%)" << endl;
        cout << "t_gpu_advanceSynapses: " << t_gpu_advanceSynapses << " ms (" << t_gpu_advanceSynapses / total_time * 100 << "%)" << endl;
        cout << "t_gpu_calcSummation: " << t_gpu_calcSummation << " ms (" << t_gpu_calcSummation / total_time * 100 << "%)" << endl;
        cout << "t_host_adjustSynapses: " << t_host_adjustSynapses << " ms (" << t_host_adjustSynapses / total_time * 100 << "%)" << endl;
        cout << "t_others: " << t_others << " ms (" << t_others / total_time * 100 << "%)" << endl;
        cout << endl;
#endif
    }

#ifdef STORE_SPIKEHISTORY
    // output spikes
    for (int i = 0; i < m_width; i++)
    {
        for (int j = 0; j < m_height; j++)
        {
            vector<uint64_t>* pSpikes = m_neuronList[i + j * m_width].getSpikes();

            DEBUG2 (cout << endl << coordToString(i, j) << endl);

            for (unsigned int i = 0; i < (*pSpikes).size(); i++)
            {
                DEBUG2 (cout << i << " ");
                int idx1 = (*pSpikes)[i] * m_deltaT;
                burstinessHist[idx1] = burstinessHist[idx1] + 1.0;
                int idx2 = (*pSpikes)[i] * m_deltaT * 100;
                spikesHistory[idx2] = spikesHistory[idx2] + 1.0;
            }
        }
    }
#endif // STORE_SPIKEHISTORY

    saveSimState(state_out, radiiHistory, ratesHistory, 
                 xloc, yloc, neuronTypes, burstinessHist, spikesHistory,
                 growthStepDuration, neuronThresh);

    // Terminate the simulator
    pSim->term(&m_si);

    // write the simulation memory image
    if (m_fWriteMemImage)
    {
        writeSimMemory(memory_out, radiiHistory, ratesHistory);
    }

    delete pSim;

#ifdef USE_OMP
    for (unsigned int i = 0; i < rgNormrnd.size(); i++)
        delete rgNormrnd[i];
#endif
    rgNormrnd.clear();
}

/**
* Clean up heap objects
*
*/
void Network::freeResources()
{
    // Empty neuron list
    if (m_rgEndogenouslyActiveNeuronMap != NULL) delete[] m_rgEndogenouslyActiveNeuronMap;
    if (m_rgSynapseMap != NULL) delete[] m_rgSynapseMap;
    if (m_rgNeuronTypeMap != NULL) delete[] m_rgNeuronTypeMap;
    if (m_summationMap != NULL) delete[] m_summationMap;
}

/**
 * Resets all of the maps.
 * Releases and re-allocates memory for each map, clearing them as necessary.
 */
void Network::reset()
{
    DEBUG(cout << "\nEntering Network::reset()";)

    freeResources();

    // Reset global simulation Step to 0
    g_simulationStep = 0;

    // initial maximum firing rate
    m_maxRate = m_targetRate / m_epsilon;

    // allocate maps
    m_rgNeuronTypeMap = new neuronType[m_cNeurons];

    // Used to assign endogenously active neurons
    m_rgEndogenouslyActiveNeuronMap = new bool[m_cNeurons];

    m_neuronList.clear();
    m_neuronList.resize(m_cNeurons);

    m_rgSynapseMap = new vector<DynamicSpikingSynapse>[m_cNeurons];

    m_summationMap = new FLOAT[m_cNeurons];

    // initialize maps
    for (int i = 0; i < m_cNeurons; i++)
    {
        m_rgEndogenouslyActiveNeuronMap[i] = false;
        m_summationMap[i] = 0;
    }

    m_si.cNeurons = m_cNeurons;
    m_si.pNeuronList = &m_neuronList;
    m_si.rgSynapseMap = m_rgSynapseMap;
    m_si.pSummationMap = m_summationMap;
    m_si.deltaT = m_deltaT;

    DEBUG(cout << "\nExiting Network::reset()";)
}

/**
 * Randomly populates the network accord to the neuron type counts and other parameters.
 * @post m_neuronList is populated.
 * @post m_rgNeuronTypeMap is populated.
 * @post m_pfStarterMap is populated.
 * @param Iinject
 * @param Inoise
 * @param Vthresh
 * @param Vresting
 * @param Vreset
 * @param Vinit
 * @param starter_Vthresh
 * @param starter_Vreset
 */
void Network::initNeurons(FLOAT Iinject[2], FLOAT Inoise[2], FLOAT Vthresh[2], FLOAT Vresting[2],
        FLOAT Vreset[2], FLOAT Vinit[2], FLOAT starter_Vthresh[2], FLOAT starter_Vreset[2])
{
    DEBUG(cout << "\nAllocating neurons..." << endl;)

    initNeuronTypeMap();
    initStarterMap();

    /* set their specific types */
    for (int i = 0; i < m_cNeurons; i++)
    {
        // set common parameters
        // Note that it is important to make the RNG calls happen in a
        // deterministic order. THIS CANNOT BE ASSURED IF THE CALLS
        // ARE WRITTEN AS PART OF THE ARGUMENTS IN A FUNCTION CALL!!
        // Thank you, C++ standards committee.
        FLOAT Ii = rng.inRange(Iinject[0], Iinject[1]);
        FLOAT In = rng.inRange(Inoise[0], Inoise[1]);
        FLOAT Vth = rng.inRange(Vthresh[0], Vthresh[1]);
        FLOAT Vrest = rng.inRange(Vresting[0], Vresting[1]);
        FLOAT Vres = rng.inRange(Vreset[0],Vreset[1]);
        FLOAT Vin = rng.inRange(Vinit[0], Vinit[1]);
        m_neuronList[i].setParams(Ii, In, Vth, Vrest, Vres, Vin, m_deltaT);

        switch (m_rgNeuronTypeMap[i])
        {
        case INH:
            DEBUG2(cout << "setting inhibitory neuron: "<< i << endl;)
            // set inhibitory absolute refractory period
            m_neuronList[i].Trefract = DEFAULT_InhibTrefract;
            break;

        case EXC:
            DEBUG2(cout << "setting exitory neuron: " << i << endl;)
            // set excitory absolute refractory period
            m_neuronList[i].Trefract = DEFAULT_ExcitTrefract;
            break;

        default:
            DEBUG2(cout << "ERROR: unknown neuron type: " << m_rgNeuronTypeMap[i] << "@" << i << endl;)
            assert(false);
        }

        if (m_rgEndogenouslyActiveNeuronMap[i])
        {
            DEBUG2(cout << "setting endogenously active neuron properties" << endl;)
            // set endogenously active threshold voltage, reset voltage, and refractory period
            m_neuronList[i].Vthresh = rng.inRange(starter_Vthresh[0], starter_Vthresh[1]);
            m_neuronList[i].Vreset = rng.inRange(starter_Vreset[0], starter_Vreset[1]);
            m_neuronList[i].Trefract = DEFAULT_ExcitTrefract;
        }
        DEBUG2(cout << m_neuronList[i].toStringAll() << endl;)
    }
    DEBUG(cout << "Done initializing neurons..." << endl;)
}

/**
 * Randomly populates the m_rgNeuronTypeMap with the specified number of inhibitory and
 * excitory neurons.
 * @post m_rgNeuronTypeMap is populated.
 */
void Network::initNeuronTypeMap()
{
    DEBUG(cout << "\nInitializing neuron type map"<< endl;);

    // Get random neuron list
    vector<neuronType>* randomDist = getNeuronOrder();

    // Copy the contents of randomDist into m_rgNeuronTypeMap.
    // This is an spatial locality optimization - contiguous arrays usually cause
    // fewer cache misses.
    for (int i = 0; i < m_cNeurons; i++)
    {
        m_rgNeuronTypeMap[i] = (*randomDist)[i];
        DEBUG2(cout << "neuron" << i << " as " << neuronTypeToString(m_rgNeuronTypeMap[i]) << endl;);
    }

    delete randomDist;
    DEBUG(cout << "Done initializing neuron type map" << endl;);
}

/**
 * Populates the starter map.
 * Selects \e numStarter excitory neurons and converts them into starter neurons.
 * @pre m_rgNeuronTypeMap must already be properly initialized
 * @post m_pfStarterMap is populated.
 */
void Network::initStarterMap()
{
    if (m_fFixedLayout)
    {
        for (size_t i = 0; i < m_pEndogenouslyActiveNeuronLayout->size(); i++)
        {
            m_rgEndogenouslyActiveNeuronMap[m_pEndogenouslyActiveNeuronLayout->at(i)] = true;
        }
    }
    else
    {
        int starters_allocated = 0;

        DEBUG(cout << "\nRandomly initializing starter map\n";);
        DEBUG(cout << "Total neurons: " << m_cNeurons << endl;)
        DEBUG(cout << "Starter neurons: " << m_cStarterNeurons << endl;)

        // randomly set neurons as starters until we've created enough
        while (starters_allocated < m_cStarterNeurons)
        {
            // Get a random integer
            int i = static_cast<int>(rng.inRange(0, m_cNeurons));

            // If the neuron at that index is excitatory and a starter map
            // entry does not already exist, add an entry.
            if (m_rgNeuronTypeMap[i] == EXC && m_rgEndogenouslyActiveNeuronMap[i] == false)
            {
                m_rgEndogenouslyActiveNeuronMap[i] = true;
                starters_allocated++;
                DEBUG(cout << "allocated EA neuron at random index [" << i << "]" << endl;);
            }
        }

        DEBUG(cout <<"Done randomly initializing starter map\n\n";)
    }
}

/**
 *  Creates a randomly ordered distribution with the specified numbers of neuron types.
 *  @returns A flat vector (to map to 2-d [x,y] = [i % m_width, i / m_width])
 */
vector<neuronType>* Network::getNeuronOrder()
{
    vector<neuronType>* randomlyOrderedNeurons = new vector<neuronType>(0);

    // create a vector of neuron types, defaulting to EXC
    vector<neuronType> orderedNeurons(m_cNeurons, EXC);

    if (m_fFixedLayout)
    {
        /* setup neuron types */
        DEBUG(cout << "Total neurons: " << m_cNeurons << endl;)
        DEBUG(cout << "Inhibitory Neurons: " << m_pInhibitoryNeuronLayout->size() << endl;)
        DEBUG(cout << "Excitatory Neurons: " << (m_cNeurons - m_pInhibitoryNeuronLayout->size()) << endl;)

        randomlyOrderedNeurons->resize(m_cNeurons);

        for (int i = 0; i < m_cNeurons; i++)
        {
            (*randomlyOrderedNeurons)[i] = EXC;
        }

        for (size_t i = 0; i < m_pInhibitoryNeuronLayout->size(); i++)
        {
            (*randomlyOrderedNeurons)[m_pInhibitoryNeuronLayout->at(i)] = INH;
        }
    }
    else
    {
        DEBUG(cout << "\nDetermining random ordering...\n";)

        /* setup neuron types */
        DEBUG(cout << "total neurons: " << m_cNeurons << endl;)
        DEBUG(cout << "m_cInhibitoryNeurons: " << m_cInhibitoryNeurons << endl;)
        DEBUG(cout << "m_cExcitoryNeurons: " << m_cExcitoryNeurons << endl;)

        // set the correct number to INH
        for (int i = 0; i < m_cInhibitoryNeurons; i++)
        {
            orderedNeurons[i] = INH;
        }

        // Shuffle ordered list into an unordered list
        while (!orderedNeurons.empty())
        {
            int i = static_cast<int>(rng() * orderedNeurons.size());

            neuronType t = orderedNeurons[i];
            
            DEBUG2(cout << "ordered neuron [" << i << "], type: " << orderedNeurons[i] << endl;)
            DEBUG2(cout << " allocated to random neuron [" << randomlyOrderedNeurons->size() << "]" << endl;)

            // add random neuron to back
            randomlyOrderedNeurons->push_back(t);

            vector<neuronType>::iterator it = orderedNeurons.begin(); // get iterator to ordered's front

            for (int j = 0; j < i; j++) // move it forward until it is on the pushed neuron
            {
                it++;
            }

            orderedNeurons.erase(it); // and remove that neuron from the ordered list
        }

        DEBUG(cout << "Done determining random ordering" << endl;)
    }
    return randomlyOrderedNeurons;
}

/**
* Save current simulation state to XML
*
* @param os
* @param radiiHistory
* @param ratesHistory
* @param xloc
* @param yloc
* @param neuronTypes
* @param burstinessHist
* @param spikesHistory
* @param Tsim
*/
void Network::saveSimState(ostream& os, CompleteMatrix& radiiHistory, 
                           CompleteMatrix& ratesHistory, VectorMatrix& xloc,
                           VectorMatrix& yloc, VectorMatrix& neuronTypes, 
                           VectorMatrix& burstinessHist, VectorMatrix& spikesHistory, FLOAT Tsim, VectorMatrix& neuronThresh)
{
    // Write XML header information:
    os << "<?xml version=\"1.0\" standalone=\"no\"?>\n" << "<!-- State output file for the DCT growth modeling-->\n";
    //os << version; TODO: version

    // Write the core state information:
    os << "<SimState>\n";
    os << "   " << radiiHistory.toXML("radiiHistory") << endl;
    os << "   " << ratesHistory.toXML("ratesHistory") << endl;
    os << "   " << burstinessHist.toXML("burstinessHist") << endl;
    os << "   " << spikesHistory.toXML("spikesHistory") << endl;
    os << "   " << xloc.toXML("xloc") << endl;
    os << "   " << yloc.toXML("yloc") << endl;
    os << "   " << neuronTypes.toXML("neuronTypes") << endl;

    if (m_cStarterNeurons > 0)
    {
        VectorMatrix starterNeuronsM("complete", "const", 1, m_cStarterNeurons);

        getStarterNeuronMatrix(starterNeuronsM);

        os << "   " << starterNeuronsM.toXML("starterNeurons") << endl;
    }

    // Write neuron thresold
    os << "   " << neuronThresh.toXML("neuronThresh") << endl;

    // write time between growth cycles
    os << "   <Matrix name=\"Tsim\" type=\"complete\" rows=\"1\" columns=\"1\" multiplier=\"1.0\">" << endl;
    os << "   " << Tsim << endl;
    os << "</Matrix>" << endl;

    // write simulation end time
    os << "   <Matrix name=\"simulationEndTime\" type=\"complete\" rows=\"1\" columns=\"1\" multiplier=\"1.0\">" << endl;
    os << "   " << g_simulationStep * m_deltaT << endl;
    os << "</Matrix>" << endl;
    os << "</SimState>" << endl;
}

/**
* Write the simulation memory image
*
* @param os	The filestream to write
*/
void Network::writeSimMemory(ostream& os, CompleteMatrix& radiiHistory, CompleteMatrix& ratesHistory)
{
    // write the neurons data
    os.write(reinterpret_cast<const char*>(&m_cNeurons), sizeof(m_cNeurons));
    for (int i = 0; i < m_cNeurons; i++)
    {
        m_neuronList[i].write(os);
    }

    // write the synapse data
    int synapse_count = 0;
    for (int i = 0; i < m_cNeurons; i++)
    {
        synapse_count += m_rgSynapseMap[i].size();
    }
    os.write(reinterpret_cast<const char*>(&synapse_count), sizeof(synapse_count));
    for (int i = 0; i < m_cNeurons; i++)
    {
        for (unsigned int j = 0; j < m_rgSynapseMap[i].size(); j++)
        {
            m_rgSynapseMap[i][j].write(os);
        }
    }

    // write the final radii
    for (int i = 0; i < m_cNeurons; i++)
    {
        os.write(reinterpret_cast<const char*>(&radiiHistory(m_si.currentStep, i)), sizeof(FLOAT));
    }

    // write the final rates
    for (int i = 0; i < m_cNeurons; i++)
    {
        os.write(reinterpret_cast<const char*>(&ratesHistory(m_si.currentStep, i)), sizeof(FLOAT));
    }
    os.flush();
}

/**
* Read the simulation memory image
*
* @param is	The filestream to read
*/
void Network::readSimMemory(istream& is, VectorMatrix& radii, VectorMatrix& rates)
{
    // read the neuron data
    int cNeurons;
    is.read(reinterpret_cast<char*>(&cNeurons), sizeof(cNeurons));
    assert( cNeurons == m_cNeurons );
    for (int i = 0; i < m_cNeurons; i++)
    {
        m_neuronList[i].read(is);
    }

    // read the synapse data & create synapses
    int synapse_count;
    is.read(reinterpret_cast<char*>(&synapse_count), sizeof(synapse_count));
    for (int i = 0; i < synapse_count; i++)
    {
	// read the synapse data and add it to the list
        DynamicSpikingSynapse::read(is, m_summationMap, m_width, m_rgSynapseMap);
    }

    // read the radii
    for (int i = 0; i < m_cNeurons; i++)
    {
        is.read(reinterpret_cast<char*>(&radii[i]), sizeof(FLOAT));
    }

    // read the rates
    for (int i = 0; i < m_cNeurons; i++)
    {
        is.read(reinterpret_cast<char*>(&rates[i]), sizeof(FLOAT));
    }
}

/**
* Copy neuron type array into VectorMatrix
*
* @param neuronTypes [out] Neuron type VectorMatrix
*/
void Network::getNeuronTypes(VectorMatrix& neuronTypes)
{
    for (int i = 0; i < m_cNeurons; i++)
    {
        switch (m_rgNeuronTypeMap[i])
        {
        case INH:
            neuronTypes[i] = INH;
            break;

        case EXC:
            neuronTypes[i] = EXC;
            break;

        default:
            assert(false);
        }
    }
}

/**
* Get starter neuron matrix
*
* @param matrix [out] Starter neuron matrix
*/
void Network::getStarterNeuronMatrix(VectorMatrix& matrix)
{
    int cur = 0;

    for (int x = 0; x < m_width; x++)
    {
        for (int y = 0; y < m_height; y++)
        {
            if (m_rgEndogenouslyActiveNeuronMap[x + y * m_width])
            {
                matrix[cur] = x + y * m_height;
                cur++;
            }
        }
    }

    assert (cur == m_cStarterNeurons);
}
