/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/Camera.h>
#include <vsg/io/Logger.h>
#include <vsg/io/Options.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/ui/PrintEvents.h>
#include <vsg/utils/AnimationPath.h>

using namespace vsg;

double AnimationPath::period() const
{
    if (locations.empty()) return 0.0;
    return locations.rbegin()->first - locations.begin()->first;
}

AnimationPath::Location AnimationPath::computeLocation(double time) const
{
    // check for empty locations map
    if (locations.empty()) return {};

    // check for single entry in locations map
    if (locations.size() == 1) return locations.begin()->second;

    if (mode == REPEAT)
    {
        time = locations.begin()->first + std::fmod(time - locations.begin()->first, period());
    }
    else if (mode == FORWARD_AND_BACK)
    {
        double p = period();
        double t = std::fmod(time - locations.begin()->first, p * 2.0);
        if (t <= p)
            time = locations.begin()->first + t;
        else // if (t > p)
            time = locations.begin()->first + p * 2.0 - t;
    }

    if (time <= locations.begin()->first) return locations.begin()->second;
    if (time >= locations.rbegin()->first) return locations.rbegin()->second;

    auto not_less_itr = locations.lower_bound(time);
    if (not_less_itr == locations.end()) return {};
    if (not_less_itr == locations.begin()) return not_less_itr->second;

    auto less_than_itr = not_less_itr;
    --not_less_itr;

    auto& lower = less_than_itr->second;
    auto& upper = not_less_itr->second;
    double r = (time - less_than_itr->first) / (not_less_itr->first - less_than_itr->first);

    return Location{mix(lower.position, upper.position, r), mix(lower.orientation, upper.orientation, r), mix(lower.scale, upper.scale, r)};
}

dmat4 AnimationPath::computeMatrix(double time) const
{
    auto location = computeLocation(time);
    return vsg::translate(location.position) * vsg::rotate(location.orientation) * vsg::scale(location.scale);
}

void AnimationPath::read(Input& input)
{
    vsg::Object::read(input);

    if (input.version_greater_equal(1, 0, 10))
        input.readValue<uint32_t>("mode", mode);

    auto numLocations = input.readValue<uint32_t>("NumLocations");

    locations.clear();
    for (uint32_t i = 0; i < numLocations; ++i)
    {
        double time;
        Location location;
        input.read("time", time);
        input.read("position", location.position);
        input.read("orientation", location.orientation);
        input.read("scale", location.scale);
        locations[time] = location;
    }
}

void AnimationPath::write(Output& output) const
{
    vsg::Object::write(output);

    if (output.version_greater_equal(1, 0, 10))
        output.writeValue<uint32_t>("mode", mode);

    output.writeValue<uint32_t>("NumLocations", locations.size());
    for (auto& [time, location] : locations)
    {
        output.write("time", time);
        output.write("position", location.position);
        output.write("orientation", location.orientation);
        output.write("scale", location.scale);
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// AnimationPathHandler
//
AnimationPathHandler::AnimationPathHandler(ref_ptr<Object> in_object, ref_ptr<AnimationPath> in_path, clock::time_point in_start_point) :
    object(in_object),
    path(in_path),
    startPoint(in_start_point),
    statsStartPoint(in_start_point)
{
}

void AnimationPathHandler::apply(Camera& camera)
{
    auto lookAt = camera.viewMatrix.cast<LookAt>();
    if (lookAt)
    {
        lookAt->set(path->computeMatrix(time));
    }
}

void AnimationPathHandler::apply(MatrixTransform& transform)
{
    transform.matrix = path->computeMatrix(time);
}

void AnimationPathHandler::apply(KeyPressEvent& keyPress)
{
    if (keyPress.keyBase == resetKey)
    {
        // reset main animation time back to start
        startPoint = keyPress.time;
        time = 0.0;

        // reset stats time back to start
        statsStartPoint = startPoint;
        frameCount = 0;
    }
}

void AnimationPathHandler::apply(FrameEvent& frame)
{
    // update the main animation time and apply it to the attached object (Camera or Transform)
    time = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - startPoint).count();
    if (object) object->accept(*this);

    if (printFrameStatsToConsole)
    {
        double statsTime = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - statsStartPoint).count();
        ++frameCount;
        if (statsTime > path->period())
        {
            double averageFramerate = static_cast<double>(frameCount) / statsTime;
            vsg::info("Animation path complete: duration = ", statsTime, ", frame count = ", frameCount, ", average frame rate = ", averageFramerate);

            // reset stats time back to start
            statsStartPoint = frame.time;
            frameCount = 0;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// RecordAnimationPathHandler
//
RecordAnimationPathHandler::RecordAnimationPathHandler(ref_ptr<Object> in_object, const Path& in_filename, ref_ptr<Options> in_options) :
    object(in_object),
    filename(in_filename),
    options(in_options)
{
    if (filename)
    {
        path = vsg::read_cast<AnimationPath>(filename, options);
    }
    else
    {
        filename = "saved_animation.vsgt";
    }

    if (!path) path = AnimationPath::create();
}

void RecordAnimationPathHandler::apply(Camera& camera)
{
    if (playing)
    {
        auto lookAt = camera.viewMatrix.cast<LookAt>();
        if (lookAt)
        {
            lookAt->set(path->computeMatrix(time));
        }
    }
    else if (recording)
    {
        dvec3 position, scale;
        dquat orientation;
        auto matrix = camera.viewMatrix->inverse();
        if (decompose(matrix, position, orientation, scale))
        {
            path->add(time, position, orientation, scale);
        }
    }
}

void RecordAnimationPathHandler::apply(MatrixTransform& transform)
{
    if (playing)
    {
        transform.matrix = path->computeMatrix(time);
    }
    else if (recording)
    {
        dvec3 position, scale;
        dquat orientation;
        if (decompose(transform.matrix, position, orientation, scale))
        {
            path->add(time, position, orientation, scale);
        }
    }
}

void RecordAnimationPathHandler::apply(KeyPressEvent& keyPress)
{
    if (keyPress.keyModified == togglePlaybackKey)
    {
        if (!playing && path->locations.size() > 1)
        {
            info("Starting playback.");

            recording = false;
            playing = true;

            // reset main animation time back to start
            startPoint = keyPress.time;
            time = 0.0;

            // reset stats time back to start
            statsStartPoint = startPoint;
            frameCount = 0;
        }
        else
        {
            playing = false;
        }
    }
    else if (keyPress.keyModified == toggleRecordingKey)
    {
        if (!recording)
        {
            info("Starting recording.");

            // reset main animation time back to start
            startPoint = keyPress.time;
            time = 0.0;

            // reset stats time back to start
            statsStartPoint = startPoint;
            frameCount = 0;

            recording = true;
            playing = false;

            path->locations.clear();
        }
        else
        {
            info("Stop recording.");

            if (filename)
            {
                if (vsg::write(path, filename, options))
                {
                    info("Written recoded path to : ", filename);
                }
            }

            recording = false;
            playing = false;
        }
    }
}

void RecordAnimationPathHandler::apply(FrameEvent& frame)
{
    if (!object) return;

    // update the main animation time and apply it to the attached object (Camera or Transform)
    time = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - startPoint).count();
    if (playing || recording)
    {
        object->accept(*this);

        if (playing && printFrameStatsToConsole)
        {
            double statsTime = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - statsStartPoint).count();
            ++frameCount;
            if (statsTime > path->period())
            {
                double averageFramerate = static_cast<double>(frameCount) / statsTime;
                vsg::info("Animation path complete: duration = ", statsTime, ", frame count = ", frameCount, ", average frame rate = ", averageFramerate);

                if (path->mode == AnimationPath::ONCE) playing = false;

                // reset stats time back to start
                statsStartPoint = frame.time;
                frameCount = 0;
            }
        }
    }
}
