/*
 * Copyright © 2012, United States Government, as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All rights reserved.
 * 
 * The NASA Tensegrity Robotics Toolkit (NTRT) v1 platform is licensed
 * under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

/**
 * @file Escape_T6Controller.cpp
 * @brief Escape Controller for T6 
 * @author Steven Lessard
 * @version 1.0.0
 * $Id$
 */

// This module
#include "Escape_T6Controller.h"
// This application
#include "Escape_T6Model.h"
// This library
#include "core/tgLinearString.h"
// For AnnealEvolution
#include "learning/Configuration/configuration.h"
// The C++ Standard Library
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

# define M_PI 3.14159265358979323846 
                               
using namespace std;

//Constructor using the model subject and a single pref length for all muscles.
//Currently calibrated to decimeters
Escape_T6Controller::Escape_T6Controller(const double initialLength)
{
    this->m_initialLengths=initialLength;
    this->m_totalTime=0.0;
}

/** Set the lengths of the muscles and initialize the learning adapter */
void Escape_T6Controller::onSetup(Escape_T6Model& subject)
{
    double dt = 0.0001;

    //Fetch all the muscles and set their initial lengths
    const std::vector<tgLinearString*> muscles = subject.getAllMuscles();
    for (size_t i = 0; i < muscles.size(); ++i)
    {
        tgLinearString * const pMuscle = muscles[i];
        assert(pMuscle != NULL);
        pMuscle->setRestLength(this->m_initialLengths, dt);
    }

    setupAdapter();
}

void Escape_T6Controller::onStep(Escape_T6Model& subject, double dt)
{
    if (dt <= 0.0)
    {
        throw std::invalid_argument("dt is not positive");
    }
    m_totalTime+=dt;

    const std::vector<tgLinearString*> muscles = subject.getAllMuscles();
    setPreferredMuscleLengths(muscles, dt);
    
    //Move motors for all the muscles
    for (size_t i = 0; i < muscles.size(); ++i)
    {
        tgLinearString * const pMuscle = muscles[i];
        assert(pMuscle != NULL);
        pMuscle->moveMotors(dt);
    }

    //vector<double> state=getState();
    vector< vector<double> > actions;

    //get the actions (between 0 and 1) from evolution (todo)
    //actions=evolutionAdapter.step(dt,state);

    //instead, generate it here for now!
    for(int i=0; i<muscles.size(); i++)
    {
        vector<double> tmp;
        for(int j=0;j<2;j++)
        {
            tmp.push_back(0.5);
        }
        actions.push_back(tmp);
    }

    //transform them to the size of the structure
    actions = transformActions(actions);

    //apply these actions to the appropriate muscles according to the sensor values
    //	applyActions(subject,actions);

}

// So far, only score used for eventual fitness calculation of an Escape Model
// is the maximum distance from the origin reached during that subject's episode
void Escape_T6Controller::onTeardown(Escape_T6Model& subject) {
    std::vector<double> scores; //scores[0] == maxDistReached, scores[1] == energySpent
    double maxDistReached = 60.0; //TODO: Change to variable
    double energySpent = totalEnergySpent(subject);

    //Invariant: For now, scores must be of size 2 (as required by endEpisode())
    scores.push_back(maxDistReached);
    scores.push_back(energySpent);

    std::cout << "Tearing down" << std::endl;
    evolutionAdapter.endEpisode(scores);

    // If any of subject's dynamic objects need to be freed, this is the place to do so
}

//Scale actions according to Min and Max length of muscles.
vector< vector <double> > Escape_T6Controller::transformActions(vector< vector <double> > actions)
{
    double min=6;
    double max=11;
    double range=max-min;
    double scaledAct;
    for(int i=0;i<actions.size();i++)
    {
        for(int j=0;j<actions[i].size();j++)
        {
            scaledAct=actions[i][j]*(range)+min;
            actions[i][j]=scaledAct;
        }
    }
    return actions;
}

//Pick particular muscles (according to the structure's state) and apply the given actions one by one
void Escape_T6Controller::applyActions(Escape_T6Model& subject, vector< vector <double> > act)
{
    const std::vector<tgLinearString*> muscles = subject.getAllMuscles();
    if(act.size() != muscles.size())
    {
        cout<<"Warning: # of muscles: "<< muscles.size() << " != # of actions: "<< act.size()<<endl;
        return;
    }
    //Apply actions (currently in a random order)
    for (size_t i = 0; i < muscles.size(); ++i)
    {
        tgLinearString * const pMuscle = muscles[i];
        assert(pMuscle != NULL);
        pMuscle->setPrefLength(act[i][0]);
    }
}

void Escape_T6Controller::setupAdapter() {
    string suffix = "_Escape";
    string configAnnealEvolution = "Config.ini";
    AnnealEvolution* evo = new AnnealEvolution(suffix, configAnnealEvolution);
    bool isLearning = true;
    configuration configEvolutionAdapter;
    configEvolutionAdapter.readFile(configAnnealEvolution);

    evolutionAdapter.initialize(evo, isLearning, configEvolutionAdapter);
}

double Escape_T6Controller::totalEnergySpent(Escape_T6Model& subject) {
    double totalEnergySpent=0;

    vector<tgLinearString* > tmpStrings = subject.getAllMuscles();
    for(int i=0; i<tmpStrings.size(); i++)
    {
        tgBaseString::BaseStringHistory stringHist = tmpStrings[i]->getHistory();

        for(int j=1; j<stringHist.tensionHistory.size(); j++)
        {
            const double previousTension = stringHist.tensionHistory[j-1];
            const double previousLength = stringHist.restLengths[j-1];
            const double currentLength = stringHist.restLengths[j];
            //TODO: examine this assumption - free spinning motor may require more power         
            double motorSpeed = (currentLength-previousLength);
            if(motorSpeed > 0) // Vestigial code
                motorSpeed = 0;
            const double workDone = previousTension * motorSpeed;
            totalEnergySpent += workDone;
        }
    }
    return totalEnergySpent;
}

// Pre-condition: every element in muscles must be defined
// Post-condition: every muscle will have a new target length
void Escape_T6Controller::setPreferredMuscleLengths(std::vector<tgLinearString*> muscles, double dt) {
    for (size_t i = 0; i < muscles.size(); i++) {
        tgLinearString *const pMuscle = muscles[i];
        assert(pMuscle != NULL);

        double amplitude = 0.95 * m_initialLengths;
        double angularFrequency = 1000 * dt;
        double phase = 0; //TODO: Determined based on cluster
        double newLength = m_initialLengths;//TODO: Change to: amplitude * sin(angularFrequency * m_totalTime + phase);

        pMuscle->setRestLength(newLength, dt);
    }
}
