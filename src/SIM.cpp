#include "SIM.h"

#include <algorithm>
#include <ctime>
#include <cxxtools/csvdeserializer.h>
#include <cxxtools/decomposer.h>
#include <cxxtools/jsonformatter.h>
#include <cxxtools/utf8codec.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

#include "OccurrenceMatrix.h"

// TODO [CMP] put in a good place
namespace
{
    const unsigned int MIN_RANGE_SIZE = 2;
    const unsigned int MIN_SOLID_BLOCK_SEQUENCE_SIZE = 2;
    const unsigned long MAX_SOLID_BLOCKS_PER_SOLID_SEQUENCE = 5000;
    const unsigned long MAX_FAST_SOLID_BLOCKS_PER_SOLID_SEQUENCE = 10000;

    float getSecs(const time_t _time)
    {
        return floor(double(clock() - _time) / CLOCKS_PER_SEC * 100) / 100;
    }
}

SIM::SIM()
{
    m_min_spatial_freq = 0.0;
    m_min_block_freq = 0.0;
}

SIM::~SIM()
{
}

void SIM::clear()
{
    m_database.clear();
    m_solid_sequences.clear();
    m_ranged_sequence_positions.clear();
    m_solid_sequence_blocks.clear();
}

void SIM::run(
    const std::string & _input_filename,
    const std::string & _log_filename,
    const unsigned int & _min_spatial_freq_perc,
    const unsigned int & _min_block_freq_perc)
{
    clear();

    setMinSpatialFreq(float(_min_spatial_freq_perc) / 100.0);
    setMinBlockFreq(float(_min_block_freq_perc) / 100.0);

    clock_t start_time = clock();

    // initialize the logger
    m_log_stream.open(_log_filename.c_str());

    // logging info
    m_log_stream << "Input: " << _input_filename << std::endl;
    m_log_stream << "Log: " << _log_filename << std::endl;
    m_log_stream << "Min Spatial Frequency value: "
                 << m_min_spatial_freq << std::endl;
    m_log_stream << "Min Block Frequency value: "
                 << m_min_block_freq << std::endl;
    m_log_stream << std::endl;

    loadDatabase(_input_filename);

    SetItems items;
    generateTheSetOfAllDatabaseItems(m_database, items);

    ListCandidates candidates;
    generate1SizeCandidates(items, 0, m_database.size() - 1, candidates);

    Size seq_size = 1;

    do
    {
        m_log_stream << "* Iteration for sequence of size: "
                     << seq_size << std::endl;

        ListRangedSequence & solid_sequences_k =
            m_solid_sequences[seq_size] = ListRangedSequence();

        updateKernelsOfAllCandidates(candidates);
        mergeKernelsOfAllCandidates(candidates, solid_sequences_k);

        cleanupSolidSequencesWithSmallRangeSize(
            MIN_RANGE_SIZE, solid_sequences_k);

        updateMatchingPositions(solid_sequences_k);

        candidates.clear();
        generateCandidates(solid_sequences_k, candidates);

        ++seq_size;
    }
    while(candidates.size() > 0);

    // print solid sequences data and positions
    // printSolidSequences();

    detectBlocksOfAllSolidSequences();

    // print solid blocks data and positions
    // printSolidBlocks();

    m_log_stream << "* Total run time: "
                 << getSecs(start_time)
                 << " secs." << std::endl;

    // close the logger
    m_log_stream.close();
}

void SIM::updateKernelsOfAllCandidates(ListCandidates & _candidates)
{
    clock_t time = clock();
    m_log_stream << " - Updating candidate kernels...";

    Point db_position = 0;
    Database::const_iterator db_it;
    ListCandidates::iterator cand_it;

    // for each Database series
    for(db_it = m_database.begin(); db_it != m_database.end(); ++db_it)
    {
        // for each candidate...
        for(cand_it = _candidates.begin();
            cand_it != _candidates.end(); ++cand_it)
        {
            // ... having range containing the current series
            if(cand_it->range().contains(db_position))
            {
                // update the current candidate kernels
                // for the current series
                cand_it->updateCandidateKernels(
                    *db_it, db_position, m_min_spatial_freq);
            }
        }

        ++db_position;
    }

    m_log_stream << " (" << getSecs(time) << "s)." << std::endl;
}

void SIM::mergeKernelsOfAllCandidates(
    ListCandidates & _candidates,
    ListRangedSequence & _solid_sequences_k)
{
    clock_t time = clock();
    m_log_stream << " - Merging kernels and creating solid sequences...";

    ListKernels::const_iterator kern_it;
    ListCandidates::iterator cand_it;

    // for each candidate
    for(
        cand_it = _candidates.begin();
        cand_it != _candidates.end(); ++cand_it)
    {
        // find optimal spatial ranges for the current candidate
        cand_it->mergeKernels(m_min_spatial_freq);

        // for each optimal range
        for(
            kern_it = cand_it->kernels().begin();
            kern_it != cand_it->kernels().end();
            ++ kern_it)
        {
            // defining a new Ranged Sequence
            // that is also a Solid Sequence for this kernel range
            _solid_sequences_k.push_back(
                RangedSequence(
                    cand_it->sequence(),
                    Range(kern_it->start(), kern_it->end()),
                    kern_it->support()));
        }
    }

    m_log_stream << " (" << getSecs(time) << "s)." << std::endl;
    m_log_stream << "   (Num of solid sequences (with range size > 1)"
                 << " for this iteration: "
                 << _solid_sequences_k.size() << ")" << std::endl;
}

void SIM::detectBlocksOfAllSolidSequences()
{
    clock_t time = clock();
    m_log_stream << " - Detecting solid sequence blocks..." << std::endl;

    MapRangedSequencesByLength::const_iterator it_ss_by_len;
    ListRangedSequence::const_iterator it_ss;

    // for each length
    for(
        it_ss_by_len = m_solid_sequences.begin();
        it_ss_by_len != m_solid_sequences.end();
        ++it_ss_by_len
        )
    {
        const Size & size = it_ss_by_len->first;
        const ListRangedSequence & sequences = it_ss_by_len->second;

        ListSequenceBlocks & sequence_blocks =
            m_solid_sequence_blocks[size] = ListSequenceBlocks();

        // for each sequence of that length
        for(it_ss = sequences.begin(); it_ss != sequences.end(); ++it_ss)
        {
            detectSolidSequenceBlocksFromSolidSequence(
                *it_ss, m_min_block_freq, sequence_blocks);
        }
    }

    m_log_stream << " (" << getSecs(time) << "s)." << std::endl;
}

void SIM::loadDatabase(const std::string & _input_filename)
{
    clock_t time = clock();
    m_log_stream << "Loading database...";

    std::ifstream file_stream;
    file_stream.open(_input_filename.c_str(), std::ifstream::in);

    cxxtools::CsvDeserializer deserializer(file_stream);
    deserializer.delimiter(',');
    deserializer.deserialize(m_database);

    file_stream.close();

    m_log_stream << " (" << getSecs(time) << "s)." << std::endl;
}

void SIM::generateTheSetOfAllDatabaseItems(
    const Database & _database,
    SetItems & _items)
{
    clock_t time = clock();
    m_log_stream << "Generating a set with all database items...";

    // from http://stackoverflow.com/a/1041939/846686
    // and http://stackoverflow.com/a/24477023/846686
    Database::const_iterator db_it;
    Serie::const_iterator sr_it;

    for(db_it = _database.begin(); db_it != _database.end(); ++db_it)
    {
        for(sr_it = db_it->begin(); sr_it != db_it->end(); ++sr_it)
        {
            _items.insert(*sr_it);
        }
    }

    m_log_stream << " (" << getSecs(time) << "s)." << std::endl;
}

void SIM::generate1SizeCandidates(
    const SetItems & _items,
    const Point & _start,
    const Point & _end,
    ListCandidates & _candidates)
{
    clock_t time = clock();
    m_log_stream << "Generating 1-size candidates...";

    SetItems::const_iterator it;

    // for each item in the Database
    for(it = _items.begin(); it != _items.end(); ++it)
    {
        // generate a 1-size candidate associated to an empty set of kernels
        _candidates.push_back(
            Candidate(
                Sequence(*it),
                Range(_start, _end),
                ListKernels()
                ));
    }

    m_log_stream << " (" << getSecs(time) << "s)." << std::endl;
    m_log_stream << "(Num of candidates: "
                 << _candidates.size() << ")" << std::endl;
}

void SIM::generateCandidates(
    const ListRangedSequence & _solid_sequences,
    ListCandidates & _candidates)
{
    clock_t time = clock();
    m_log_stream << " - Generating candidates...";

    ListRangedSequence::const_iterator x_it, y_it;
    Sequence seq1_without_first_item;
    Sequence seq2_without_last_item;
    Sequence seq_joined;
    Range rg_intersect(0, 0);

    if(_candidates.size() > 0)
    {
        std::stringstream msg;
        msg << "generateCandidates expects"
            << " an empty list of candidates as input."
            << std::endl;
        throw std::runtime_error(msg.str());
    }

    // for each (x, y) permutation of SSset k
    // where all x sequence items excluding the first
    // are equal to all y sequence items excluding the last
    // and skipping generation of candidates
    // with ranges containing just one time series
    for(
        x_it = _solid_sequences.begin();
        x_it != _solid_sequences.end();
        ++x_it
        )
    {
        for(
            y_it = _solid_sequences.begin();
            y_it != _solid_sequences.end();
            ++y_it
            )
        {
            //TODO [CMP] to test
            if(x_it->range().intersects(y_it->range(), rg_intersect) &&
               rg_intersect.size() > 1
                )
            {
                seq1_without_first_item.clear();
                x_it->sequence().getSubsequenceDroppingFirstItem(
                    seq1_without_first_item);

                seq2_without_last_item.clear();
                y_it->sequence().getSubsequenceDroppingLastItem(
                    seq2_without_last_item);

                if(seq1_without_first_item == seq2_without_last_item)
                {
                    seq_joined = x_it->sequence();
                    seq_joined.append(y_it->sequence().getLast());
                    _candidates.push_back(
                        Candidate(
                            seq_joined,
                            rg_intersect,
                            ListKernels())
                        );
                }
            }
        }
    }

    m_log_stream << " (" << getSecs(time) << "s)." << std::endl;
    m_log_stream << "   (Num of candidates: " <<
        _candidates.size() << ")" << std::endl;
}

void SIM::setMinSpatialFreq(const Frequency & _min_spatial_freq)
{
    if(_min_spatial_freq <=0 )
    {
        std::stringstream msg;
        msg << "Minumum Spatial Frequency parameter value ("
            << _min_spatial_freq << ")"
            << " cannot be less then or equal to 0.";
        throw std::runtime_error(msg.str());
    }

    if(_min_spatial_freq > 1.0)
    {
        std::stringstream msg;
        msg << "Minumum Spatial Frequency parameter value ("
            << _min_spatial_freq << ")"
            << " cannot be greater then 1.0.";
        throw std::runtime_error(msg.str());
    }

    m_min_spatial_freq = _min_spatial_freq;
}

void SIM::setMinBlockFreq(const Frequency & _min_block_freq)
{
    if(_min_block_freq <= 0)
    {
        std::stringstream msg;
        msg << "Minumum Block Frequency parameter value ("
            << _min_block_freq << ")"
            << " cannot be less then or equal to 0.";
        throw std::runtime_error(msg.str());
    }

    if(_min_block_freq > 1.0)
    {
        std::stringstream msg;
        msg << "Minumum Block Frequency parameter value ("
            << _min_block_freq << ")"
            << " cannot be greater then 1.0.";
        throw std::runtime_error(msg.str());
    }

    m_min_block_freq = _min_block_freq;
}

void SIM::updateMatchingPositions(const ListRangedSequence & _solid_sequences)
{
    clock_t time = clock();
    m_log_stream << " - Detecting sequence positions in the database...";

    ListRangedSequence::const_iterator it_seq;
    unsigned int str_seq_size;
    std::string str_seq_rep;
    std::stringstream ss_tmp;
    std::string::iterator str_it;
    unsigned int tot_rows = m_database.size();
    unsigned int tot_cols;
    unsigned int col;
    unsigned int start_match_col;
    bool match_started;
    //unsigned int last_match_col; // [CMP] useful only for max-gap

    // for each input data-seqeunce, check if contain the sequence
    for(unsigned int row = 0; row < tot_rows; ++row)
    {
        // [CMP] for debug
        // m_log_stream << std::string(
        //     m_database[row].begin(), m_database[row].end()) << std::endl;

        tot_cols = m_database[row].size();

        // for each sequence candidate
        for(
            it_seq = _solid_sequences.begin();
            it_seq != _solid_sequences.end();
            ++it_seq
            )
        {
            if(! it_seq->range().contains(row))
            {
                continue;
            }

            // obtaining a string representation of the sequence
            // to easy loop inside it's elements
            str_seq_rep = it_seq->sequence().toStringOfItems();
            str_seq_size = it_seq->sequence().size();

            // if(str_seq_size < 2)
            // {
            //     std::stringstream msg;
            //     msg << "Current support count function doesn't handle"
            //         << " sequence with less then two items." << std::endl
            //         << "Current sequence: '" << str_seq_rep << "'.";
            //     throw std::runtime_error(msg.str());
            // }

            if(m_ranged_sequence_positions.find(& (*it_seq))
               == m_ranged_sequence_positions.end())
            {
                m_ranged_sequence_positions[& (*it_seq)] =
                    ListPositions();
            }

            str_it = str_seq_rep.begin();

            col = 0;
            match_started = false;
            //last_match_col = 0; // [CMP] useful only for max-gap

            // passing through every item of the row
            while(col < tot_cols)
            {
                // check the max_time window constrant
                // this check must be an early check
                if(
                    // [CMP] this is useful only for max-gap
                    //last_match_col != 0 &&
                    //(col - last_match_col) > (m_max_gap + 1)

                    // [CMP] imagine a sequence of length 5
                    // max_time_window < 5 make no sense
                    match_started &&
                    // [CMP]
                    // (col - start_match_col) >= (
                    //     m_max_time_window + str_seq_size)
                    (col - start_match_col >= str_seq_size)
                    )
                {
                    // rewind to start_match_col + 1
                    col = start_match_col + 1;
                    match_started = false;
                    //last_match_col = 0; // [CMP] useful only for max-gap
                    str_it = str_seq_rep.begin();
                    continue;
                }

                Item & item = m_database[row][col];

                // if match, go to next item of the sequence
                if(item == *str_it)
                {
                    // take note for the start of the match
                    if(! match_started)
                    {
                        start_match_col = col;
                        match_started = true;
                    }

                    //last_match_col = col; // [CMP] useful only for max-gap
                    ++str_it;

                    // if the sequence iterator reach the end,
                    // then the input data-sequence contain the sequence
                    if(str_it == str_seq_rep.end())
                    {
                        m_ranged_sequence_positions[& (*it_seq)]
                            .push_back(Position(row, col - str_seq_size + 1));

                        // rewind to start_match_col + 1
                        col = start_match_col + 1;
                        match_started = false;
                        //last_match_col = 0; // [CMP] useful only for max-gap
                        str_it = str_seq_rep.begin();
                        continue;
                    }
                }

                // in any other case
                // go to the next item of the input data-sequence
                ++col;
            }
        }
    }

    m_log_stream << " (" << getSecs(time) << "s)." << std::endl;
}

void SIM::cleanupSolidSequencesWithSmallRangeSize(
    const Size & _min_size, ListRangedSequence & _solid_sequences)
{
    clock_t time = clock();
    m_log_stream << " - Clean up solid sequences with small range size...";

    ListRangedSequence::iterator it = _solid_sequences.begin();

    while(it != _solid_sequences.end())
    {
        if(it->range().size() < _min_size)
        {
            it = _solid_sequences.erase(it);
        }
        else
        {
            ++it;
        }
    }

    m_log_stream << " (" << getSecs(time) << "s)." << std::endl;
}

void SIM::detectSolidSequenceBlocksFromSolidSequence(
    const RangedSequence & _solid_sequence,
    const Frequency & _min_block_freq,
    ListSequenceBlocks & _sequence_blocks)
{
    if(_solid_sequence.sequence().size() < MIN_SOLID_BLOCK_SEQUENCE_SIZE)
    {
        return;
    }

    m_log_stream << '\t'
                 << _solid_sequence.sequence().toStringOfItems() << '('
                 << _solid_sequence.range().start() << ','
                 << _solid_sequence.range().end() << ')';
    m_log_stream.flush();

    const ListPositions & positions = m_ranged_sequence_positions.find(
        & _solid_sequence)->second;

    OccurrenceMatrix occurr_matrix(
        _solid_sequence,
        positions);

    ListSequenceBlocks sb_candidates;
    generate1SizeBlockCandidatesForEachSequenceOccurrence(
        positions,
        _solid_sequence,
        sb_candidates);

    m_log_stream << "\tinit.size: " << sb_candidates.size();
    m_log_stream.flush();

    time_t time;

    ListSequenceBlocks to_add;
    ListSequenceBlocks::iterator it_sb_to_add;

    typedef std::set <
        ListSequenceBlocks::iterator, SequenceBlock::LessThanComparer > DelIt;
    DelIt to_del;
    DelIt::iterator it_sb_to_del;

    ListSequenceBlocks::iterator it_sb_q, it_sb_r;

    Range new_range(0, 0);
    Interval new_interval(0,0);
    Support new_support;
    Size new_area;

    bool did_any_merge;
    bool is_contained;
    ListSequenceBlocks::iterator chk_toadd_bigger_it;

	if(sb_candidates.size() < MAX_SOLID_BLOCKS_PER_SOLID_SEQUENCE)
    {
        m_log_stream << "\t full merges:";
        m_log_stream.flush();

        size_t additional_erased;
        size_t skipped;
        ListSequenceBlocks::iterator chk_cand_bigger_it;

        // TODO [CMP] sort disabled: is wrost
        // ListSequenceBlocks sb_sub_sort;
        // ListSequenceBlocks::iterator it_sort_start, it_sort_end;

        // merge the Sequence Block candidates to obtain Solid Sequence Blocks
        // that respect Θ
        do
        {
            did_any_merge = false;

            // TODO [CMP] sort disabled: is wrost
            // time = clock();

            // sort candidates to be ordered by Manhattan distance
            // from 0,0 point
            // sb_candidates.sort(
            //     SequenceBlock::PositionComparer(0, 0));

            // m_log_stream << " sort"<< " (" << getSecs(time) << "s)";
            // m_log_stream.flush();

            time = clock();

            // for each solid block q
            for(
                it_sb_q = sb_candidates.begin();
                it_sb_q != sb_candidates.end();
                ++it_sb_q)
            {
                // TODO [CMP] sort disabled: is wrost
                // sort candidates to be ordered by Manhattan distance
                // from 0,0 point
                // it_sort_start = it_sb_q;
                // ++it_sort_start;
                // it_sort_end = sb_candidates.end();

                // // transfer to a temporary list
                // sb_candidates.splice(
                //     sb_sub_sort.begin(),
                //     sb_sub_sort,
                //     it_sort_start,
                //     it_sort_end);

                // // sort
                // sb_sub_sort.sort(
                //     SequenceBlock::PositionComparer(
                //         it_sb_q->range().start(),
                //         it_sb_q->interval().start()));

                // // transfer back
                // sb_sub_sort.splice(
                //     sb_candidates.end(),
                //     sb_candidates,
                //     sb_sub_sort.begin(),
                //     sb_sub_sort.end());

                // for each other solid block r... where r > q
                for(
                    it_sb_r = sb_candidates.end(), --it_sb_r;
                    it_sb_r != it_sb_q;
                    --it_sb_r)
                {
                    it_sb_q->range().unify(it_sb_r->range(), new_range);
                    it_sb_q->interval().unify(
                        it_sb_r->interval(), new_interval);

                    is_contained = false;

                    // the check is done only for to_add
                    // because it is very slow to do for sb_candidates too
                    for(
                        chk_toadd_bigger_it = to_add.begin();
                        chk_toadd_bigger_it != to_add.end();
                        ++chk_toadd_bigger_it)
                    {
                        if(chk_toadd_bigger_it->contains(
                               new_range, new_interval))
                        {
                            is_contained = true;
                            break;
                        }
                    }

                    if(! is_contained)
                    {
                        new_support = occurr_matrix.getSupport(
                            new_range, new_interval);
                        new_area = new_range.size() * new_interval.size();

                        // if num of sequence items in the block divided by
                        // the num of all items in the block is >= Θ
                        if((float(new_support) / new_area) >= _min_block_freq)
                        {
                            SequenceBlock merged(
                                _solid_sequence.sequence(),
                                new_range,
                                new_interval,
                                new_support);

                            if(merged.hasSameCoordinates(* it_sb_q))
                            {
                                m_log_stream << "=";
                                to_del.insert(it_sb_r);
                            }
                            else if(merged.hasSameCoordinates(* it_sb_r))
                            {
                                m_log_stream << "=";
                                to_del.insert(it_sb_q);
                            }
                            else
                            {
                                to_del.insert(it_sb_q);
                                to_del.insert(it_sb_r);

                                // add only merges
                                // not already contained in the block to_add
                                is_contained = false;
                                chk_toadd_bigger_it = to_add.begin();

                                while(chk_toadd_bigger_it != to_add.end())
                                {
                                    if(chk_toadd_bigger_it->contains(merged))
                                    {
                                        is_contained = true;
                                        ++chk_toadd_bigger_it;
                                    }
                                    else if (merged.contains(
                                                 * chk_toadd_bigger_it))
                                    {
                                        to_add.erase(chk_toadd_bigger_it++);
                                    }
                                    else
                                    {
                                        ++chk_toadd_bigger_it;
                                    }
                                }

                                if(! is_contained)
                                {
                                    to_add.push_back(merged);
                                    did_any_merge = true;
                                }
                            }
                        }
                    }
                }
            }

            m_log_stream << " +" << to_add.size() << " -" << to_del.size()
                         << " (" << getSecs(time) << "s)";
            m_log_stream.flush();

            for(
                it_sb_to_del = to_del.begin();
                it_sb_to_del != to_del.end();
                ++it_sb_to_del)
            {
                sb_candidates.erase(* it_sb_to_del);
            }

            to_del.clear();

            time = clock();
            additional_erased = 0;
            skipped = 0;

            for(
                it_sb_to_add = to_add.begin();
                it_sb_to_add != to_add.end();
                ++it_sb_to_add)
            {
                // add only not already contained in the block candidates
                is_contained = false;
                chk_cand_bigger_it = sb_candidates.begin();

                while(chk_cand_bigger_it != sb_candidates.end())
                {
                    if(chk_cand_bigger_it->contains(* it_sb_to_add))
                    {
                        is_contained = true;
                        ++chk_cand_bigger_it;
                        ++skipped;
                    }
                    else if (it_sb_to_add->contains(* chk_cand_bigger_it))
                    {
                        sb_candidates.erase(chk_cand_bigger_it++);
                        ++additional_erased;
                    }
                    else
                    {
                        ++chk_cand_bigger_it;
                    }
                }

                if(! is_contained)
                {
                    sb_candidates.push_back(* it_sb_to_add);
                }
            }

            to_add.clear();

            if(sb_candidates.size() > MAX_FAST_SOLID_BLOCKS_PER_SOLID_SEQUENCE)
            {
                m_log_stream << " [WARN] max fast blocks per sequence "
                             << "exceeded: " << sb_candidates.size()
                             << ". Skip." << std::endl;
                return;
            }

            m_log_stream << " --" << additional_erased << " #"
                         << skipped << " (" << getSecs(time) << "s) |";
            m_log_stream.flush();
        }
        while(did_any_merge);
	}
	else
	{
        m_log_stream << "\t fast merges:";
        m_log_stream.flush();

        Size add_count;
        Size del_count;

        bool current_q_used_to_merge;

        // merge the Sequence Block candidates to obtain Solid Sequence Blocks
        // that respect Θ
        do
        {
            did_any_merge = false;

            add_count = 0;
            del_count = 0;

            time = clock();

            // for each solid block q
            for(
                it_sb_q = sb_candidates.begin();
                it_sb_q != sb_candidates.end();
                ++it_sb_q)
            {
                current_q_used_to_merge = false;

                // for each other solid sequence r... where r > q
                for(
                    it_sb_r = sb_candidates.end(), --it_sb_r;
                    it_sb_r != it_sb_q;
                    --it_sb_r)
                {
                    it_sb_q->range().unify(it_sb_r->range(), new_range);
                    it_sb_q->interval().unify(it_sb_r->interval(), new_interval);
                    new_support = it_sb_q->support() + it_sb_r->support();
                    new_area = new_range.size() * new_interval.size();

                    // if num of sequence items in the block divided by
                    // the num of all items in the block is >= Θ
                    if((float(new_support) / new_area) >= _min_block_freq)
                    {
                        SequenceBlock merged(
                            _solid_sequence.sequence(),
                            new_range,
                            new_interval,
                            new_support);

                        if(merged.hasSameCoordinates(* it_sb_q))
                        {
                            to_del.insert(it_sb_r);
                        }
                        else if(merged.hasSameCoordinates(* it_sb_r))
                        {
                            to_del.insert(it_sb_q);
                        }
                        else
                        {
                            to_del.insert(it_sb_q);
                            to_del.insert(it_sb_r);

                            // add only merges
                            // not already contained in the block to_add
                            is_contained = false;
                            chk_toadd_bigger_it = to_add.begin();

                            while(chk_toadd_bigger_it != to_add.end())
                            {
                                if(chk_toadd_bigger_it->contains(merged))
                                {
                                    is_contained = true;
                                    ++chk_toadd_bigger_it;
                                }
                                else if (merged.contains(
                                             * chk_toadd_bigger_it))
                                {
                                    to_add.erase(chk_toadd_bigger_it++);
                                }
                                else
                                {
                                    ++chk_toadd_bigger_it;
                                }
                            }

                            if(! is_contained)
                            {
                                to_add.push_back(merged);
                                did_any_merge = true;
                                current_q_used_to_merge = true;

                                // the current q was used in a merge,
                                // so it can not be used with any r
                                break;
                            }
                        }
                    }
                }

                if(current_q_used_to_merge)
                {
                    // current q was used, we need to continue from q=r+1
                    // because current q and r can not be used anymore
                    it_sb_q = it_sb_r;
                    continue;  // q will be incremented in the for statement
                }

                add_count += to_add.size();
                del_count += to_del.size();
            }

            for(
                it_sb_to_del = to_del.begin();
                it_sb_to_del != to_del.end();
                ++it_sb_to_del)
            {
                sb_candidates.erase(* it_sb_to_del);
            }

            to_del.clear();

            for(
                it_sb_to_add = to_add.begin();
                it_sb_to_add != to_add.end();
                ++it_sb_to_add)
            {
                sb_candidates.push_back(* it_sb_to_add);
            }

            to_add.clear();

            m_log_stream << " +" << add_count << " -" << del_count
                         << " (" << getSecs(time) << "s) |";
            m_log_stream.flush();
        }
        while(did_any_merge);
	}

    m_log_stream << std::endl;

    _sequence_blocks.insert(
        _sequence_blocks.end(),
        sb_candidates.begin(),
        sb_candidates.end());
}

void SIM::generate1SizeBlockCandidatesForEachSequenceOccurrence(
    const ListPositions & _positions,
    const RangedSequence & _solid_sequence,
    ListSequenceBlocks & _sb_candidates) const
{
    const Size seq_size =  _solid_sequence.sequence().size();
    ListPositions::const_iterator it_pos;

    for(it_pos = _positions.begin(); it_pos != _positions.end(); ++it_pos)
    {
        _sb_candidates.push_back(
            SequenceBlock(
                _solid_sequence.sequence(),
                Range(it_pos->first, it_pos->first),
                Interval(it_pos->second, it_pos->second + seq_size - 1),
                seq_size));
    }
}

void SIM::printSolidSequences()
{
    m_log_stream << std::endl << "Printing solid sequences:" << std::endl;

    MapRangedSequencesByLength::const_iterator it_ss_by_len;
    ListRangedSequence::const_iterator it_ss;

    for(
        it_ss_by_len = m_solid_sequences.begin();
        it_ss_by_len != m_solid_sequences.end();
        ++it_ss_by_len
        )
    {
        const ListRangedSequence & sequences = it_ss_by_len->second;

        for(it_ss = sequences.begin(); it_ss != sequences.end(); ++it_ss)
        {
            m_log_stream
                << '\t' << it_ss->sequence().toStringOfItems()
                << '\t' << "len: " << it_ss->sequence().size()
                << '\t' << "sup: " << it_ss->support()
                << '\t' << "r.s: " << it_ss->range().start()
                << '\t' << "r.e: " << it_ss->range().end()
                << std::endl;
        }
    }
}

void SIM::printSolidBlocks()
{
    m_log_stream << std::endl << "Printing solid blocks:" << std::endl;

    MapSequenceBlocksByLength::const_iterator it_sb_by_len;
    ListSequenceBlocks::const_iterator it_sb;

    for(
        it_sb_by_len = m_solid_sequence_blocks.begin();
        it_sb_by_len != m_solid_sequence_blocks.end();
        ++it_sb_by_len
        )
    {
        const ListSequenceBlocks & blocks = it_sb_by_len->second;

        for(it_sb = blocks.begin(); it_sb != blocks.end(); ++it_sb)
        {
            m_log_stream
                << '\t' << it_sb->sequence().toStringOfItems()
                << '\t' << "len: " << it_sb->sequence().size()
                << '\t' << "sup: " << it_sb->support()
                << '\t' << "r.s: " << it_sb->range().start()
                << '\t' << "r.e: " << it_sb->range().end()
                << '\t' << "i.s: " << it_sb->interval().start()
                << '\t' << "i.e: " << it_sb->interval().end()
                << std::endl;
        }
    }
}

void SIM::saveJSON(const std::string & _output_filename) const
{
    std::ofstream output_file;
    output_file.open(_output_filename.c_str());
    cxxtools::TextOStream ts(output_file, new cxxtools::Utf8Codec());
    cxxtools::JsonFormatter formatter(ts);
    formatter.beautify(true);

    formatter.beginObject("", "");

    formatter.beginArray("solid_sequences", "");

    MapRangedSequencesByLength::const_iterator it_ss_by_len;
    ListRangedSequence::const_iterator it_ss;
    MapPositionsBySeq::const_iterator it_list_pos;
    ListPositions::const_iterator it_pos;

    // for each length in m_solid_sequences
    for(
        it_ss_by_len = m_solid_sequences.begin();
        it_ss_by_len != m_solid_sequences.end();
        ++it_ss_by_len
        )
    {
        const Size & size = it_ss_by_len->first;
        const ListRangedSequence & sequences = it_ss_by_len->second;

        formatter.beginObject("", "");
        formatter.addValueInt("length", "", size);
        formatter.beginArray("sequences", "");

        // for each sequence of that length
        for(it_ss = sequences.begin(); it_ss != sequences.end(); ++it_ss)
        {
            formatter.beginObject("", "");

            formatter.addValueStdString(
                "sequence", "", it_ss->sequence().toStringOfItems());
            formatter.addValueInt(
                "support", "", it_ss->support());
            formatter.addValueInt(
                "start", "", it_ss->range().start());
            formatter.addValueInt(
                "end", "", it_ss->range().end());

            it_list_pos = m_ranged_sequence_positions.find(& (*it_ss));

            if(it_list_pos == m_ranged_sequence_positions.end())
            {
                std::stringstream msg;
                msg << "Can not find positions for ranged sequence '"
                    << it_ss->sequence().toStringOfItems() << '('
                    << it_ss->range().start() << ','
                    << it_ss->range().end() << ")'.";
                throw std::runtime_error(msg.str());
            }

            const ListPositions & positions = it_list_pos->second;

            if(positions.size() == 0)
            {
                std::stringstream msg;
                msg << "We find 0 positions for ranged sequence '"
                    << it_ss->sequence().toStringOfItems() << '('
                    << it_ss->range().start() << ','
                    << it_ss->range().end() << ")': this is a bug.";
                throw std::runtime_error(msg.str());
            }

            // the follow strange way to structure the data
            // is needed by R language
            {
                formatter.beginArray("spaces", "");

                for(
                    it_pos = positions.begin();
                    it_pos != positions.end();
                    ++it_pos
                    )
                {
                    formatter.addValueInt(
                        "", "", it_pos->first);
                }

                formatter.finishArray();
                formatter.beginArray("times", "");

                for(
                    it_pos = positions.begin();
                    it_pos != positions.end();
                    ++it_pos
                    )
                {
                    formatter.addValueInt(
                        "", "", it_pos->second);
                }

                formatter.finishArray();
            }

            formatter.finishObject();
        }

        formatter.finishArray();
        formatter.finishObject();
    }

    formatter.finishArray();

    formatter.beginArray("solid_blocks", "");

    MapSequenceBlocksByLength::const_iterator it_sb_by_len;
    ListSequenceBlocks::const_iterator it_sb;

    // for each length in m_solid_blocks
    for(
        it_sb_by_len = m_solid_sequence_blocks.begin();
        it_sb_by_len != m_solid_sequence_blocks.end();
        ++it_sb_by_len
        )
    {
        const Size & size = it_sb_by_len->first;
        const ListSequenceBlocks & blocks = it_sb_by_len->second;

        formatter.beginObject("", "");
        formatter.addValueInt("length", "", size);
        formatter.beginArray("blocks", "");

        // for each sequence of that length
        for(it_sb = blocks.begin(); it_sb != blocks.end(); ++it_sb)
        {
            formatter.beginObject("", "");

            formatter.addValueStdString(
                "sequence", "", it_sb->sequence().toStringOfItems());
            formatter.addValueInt(
                "support", "", it_sb->support());
            formatter.addValueInt(
                "r_start", "", it_sb->range().start());
            formatter.addValueInt(
                "r_end", "", it_sb->range().end());
            formatter.addValueInt(
                "i_start", "", it_sb->interval().start());
            formatter.addValueInt(
                "i_end", "", it_sb->interval().end());

            formatter.finishObject();
        }

        formatter.finishArray();
        formatter.finishObject();
    }
    formatter.finishArray();

    formatter.finishObject();
}
