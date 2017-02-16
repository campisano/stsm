#ifndef SEQUENCE_BLOCK__H__
#define SEQUENCE_BLOCK__H__

#include <list>
#include <map>

#include "Frequency.h"
#include "Interval.h"
#include "Position.h"
#include "Range.h"
#include "Sequence.h"
#include "Size.h"
#include "Support.h"

class SequenceBlock;
typedef std::list < SequenceBlock > ListSequenceBlocks;
typedef std::map < Size, ListSequenceBlocks > MapSequenceBlocksByLength;

class SequenceBlock
{
public:
    explicit SequenceBlock(
        const Sequence & _sequence,
        const Range & _range,
        const Interval & _interval,
        const SetPositions & _positions);

    bool operator==(const SequenceBlock & _other) const;
    bool operator!=(const SequenceBlock & _other) const;

    const Sequence & sequence() const;
    const Range & range() const;
    const Interval & interval() const;

    Support support() const;
    Frequency frequency() const;

    const SetPositions & positions() const;
    Size area() const;

    bool contains(const SequenceBlock & _other) const;
    bool hasSamePositions(const SequenceBlock & _other) const;

    struct Comparer
    {
        bool operator() (
            const ListSequenceBlocks::iterator & _left,
            const ListSequenceBlocks::iterator & _right) const;
    };

private:
    Sequence m_sequence;
    Range m_range;
    Interval m_interval;

    SetPositions m_positions;
};

#endif
