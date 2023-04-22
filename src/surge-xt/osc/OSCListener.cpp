/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "OSCListener.h"
#include "Parameter.h"
#include "SurgeSynthProcessor.h"
#include <sstream>
#include <vector>
#include <string>

namespace Surge
{
namespace OSC
{

OSCListener::OSCListener() {}

OSCListener::~OSCListener()
{
    if (listening)
        stopListening();
}

bool OSCListener::init(SurgeSynthProcessor *ssp, const std::unique_ptr<SurgeSynthesizer> &surge,
                       int port)
{
    if (!connect(port))
    {
#ifdef DEBUG
        std::cout << "Error: could not connect to UDP port " << std::to_string(port) << std::endl;
#endif
        return false;
    }
    else
    {
        addListener(this);
        listening = true;
        portnum = port;
        surgePtr = surge.get();
        sspPtr = ssp;

        surgePtr->storage.oscListenerRunning = true;

#ifdef DEBUG
        std::cout << "SurgeOSC: Listening for OSC on port " << port << "." << std::endl;
#endif
        return true;
    }
}

void OSCListener::stopListening()
{
    if (!listening)
        return;

    removeListener(this);
    listening = false;

    if (surgePtr)
        surgePtr->storage.oscListenerRunning = false;

#ifdef DEBUG
    std::cout << "SurgeOSC: Stopped listening for OSC." << std::endl;
#endif
}

void OSCListener::oscMessageReceived(const juce::OSCMessage &message)
{
    std::string addr = message.getAddressPattern().toString().toStdString();
    if (addr.at(0) != '/')
        return; // ignore malformed OSC

    // Tokenize the address
    std::istringstream split(addr);
    std::vector<std::string> tokens;
    for (std::string each; std::getline(split, each, '/'); tokens.push_back(each))
        ; // first token will be blank

    // Process address tokens
    if (tokens[1] == "param")
    { // e.g. /param/volume 0.5
        std::string storage_addr = tokens[2];
        auto *p = surgePtr->storage.getPatch().parameterFromOSCName(storage_addr);
        if (p == NULL)
        {
#ifdef DEBUG
            std::cout << "No parameter with OSC or Storage name of " << storage_addr << std::endl;
#endif
            return; // Not a valid storage name
        }
        if (!message[0].isFloat32())
            return; // Not a valid data value

        sspPtr->oscRingBuf.push(SurgeSynthProcessor::oscMsg(p, message[0].getFloat32()));

#ifdef DEBUG_VERBOSE
        std::cout << "Parameter OSC name:" << p->getOSCName() << "  ";
        std::cout << "Parameter Storage name:" << p->get_storage_name() << "  ";
        std::cout << "Parameter full name:" << p->get_full_name() << std::endl;
#endif
    }

#ifdef DEBUG_VERBOSE
    std::cout << "OSCListener: Got OSC msg.; address: " << addr << "  data: ";
    for (juce::OSCArgument msg : message)
    {
        std::string dataStr = "(none)";
        switch (msg.getType())
        {
        case 'f':
            dataStr = std::to_string(msg.getFloat32());
            break;
        case 'i':
            dataStr = std::to_string(msg.getInt32());
            break;
        case 's':
            dataStr = msg.getString().toStdString();
            break;
        default:
            break;
        }
        std::cout << dataStr << "  ";
    }
    std::cout << std::endl;
#endif
}

void OSCListener::oscBundleReceived(const juce::OSCBundle &bundle)
{
    std::string msg = "";
#ifdef DEBUG
    std::cout << "OSCListener: Got OSC bundle." << msg << std::endl;
#endif

    for (int i = 0; i < bundle.size(); ++i)
    {
        auto elem = bundle[i];
        if (elem.isMessage())
            oscMessageReceived(elem.getMessage());
        else if (elem.isBundle())
            oscBundleReceived(elem.getBundle());
    }
}

} // namespace OSC
} // namespace Surge