#include "Kernel.h"

#include <sstream>
#include <stdexcept>

Kernel::Kernel(
    const Range & _range):
    Segment(_range.start(), _range.end())
{
    m_support = 0;
    m_frequency = 0;
}

Kernel::Kernel(
    const Point & _start,
    const Point & _end):
    Segment(_start, _end)
{
    m_support = 0;
    m_frequency = 0;
}

const Support & Kernel::support() const
{
    return m_support;
}

void Kernel::support(const Support & _support)
{
    Size size = m_end - m_start + 1;

    if(_support > size)
    {
        std::stringstream msg;
        msg << "Support (" << _support
            << ") can not be greater then range size ("
            << size << ").";
        throw std::runtime_error(msg.str());
    }

    m_support = _support;
}

const Frequency & Kernel::frequency() const
{
    return m_frequency;
}

void Kernel::frequency(const Frequency & _frequency)
{
    if(_frequency > 1.0)
    {
        std::stringstream msg;
        msg << "Frequenc (" << _frequency
            << ") can not be greater then 1.0.";
        throw std::runtime_error(msg.str());
    }

    m_frequency = _frequency;
}
