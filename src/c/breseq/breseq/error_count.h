/*****************************************************************************

AUTHORS

  Jeffrey E. Barrick <jeffrey.e.barrick@gmail.com>
  David B. Knoester

LICENSE AND COPYRIGHT

  Copyright (c) 2008-2010 Michigan State University
  Copyright (c) 2011 The University of Texas at Austin

  breseq is free software; you can redistribute it and/or modify it under the  
  terms the GNU General Public License as published by the Free Software 
  Foundation; either version 1, or (at your option) any later version.

*****************************************************************************/

#ifndef _BRESEQ_ERROR_COUNT_H_
#define _BRESEQ_ERROR_COUNT_H_

#include "common.h"
#include "alignment.h"
#include "pileup_base.h"

using namespace std;

namespace breseq {
	
	/*! Count errors.
	 
	 \param bam is the read file generated by breseq.
	 \param fasta is the FASTA file that corresponds to the reference sequences; generates an FAI file as needed.
	 \param output_dir is the directory in which output files will be placed.
	 \param readfiles is a list of read files that were used to build the bam (do not include filename extension)
	 */
	void error_count(const string& bam, 
									 const string& fasta,
									 const string& output_dir,
									 const vector<std::string>& readfiles,
									 bool do_coverage,
                   bool do_errors,
                   uint8_t min_qual_score,
                   const string& covariates
                    );

	/*! Error table class.

    This class is used to record observations by covariate and calibrate error rates
    Templating allows us to use the same class for counts and error frequencies/odds
	 */ 

    
  // These must be in the same order as the covariate_names[] string constants...
  enum {k_read_set, k_ref_base, k_prev_base, k_obs_base, k_quality, k_read_pos, k_base_repeat, k_num_covariates};
 
  // Range of each (where N is the unput value on command line):
  //  k_read_set = [0..N-1]
  //  k_ref_base = [0..4] (fixed)
  //  k_prev_base = [0..4] (fixed) = previous base in *reference*
  //  k_obs_base = [0..4] (fixed)
  //  k_quality_base = [0..N]
  //  k_read_pos = [0..N-1] = 0-index read position
  //  k_base_repeat = [0..N]
  
  class covariate_values_t {
    
    private:
      uint32_t m_covariates[k_num_covariates];
    
    public:
      covariate_values_t() {};
      
      //Copy constructor
      covariate_values_t(const covariate_values_t& cv) {
        memcpy(&m_covariates, &(cv.m_covariates), sizeof(m_covariates));
        //for (uint32_t i=0; i<k_num_covariates; i++) {
        //  m_covariates[i] = cv[i];
        //}
      };
    
      uint32_t& operator[](uint32_t i) { return m_covariates[i]; };
      const uint32_t operator[](uint32_t i) const { return m_covariates[i]; };

      uint32_t& read_set() {return (*this)[k_read_set]; };
      uint32_t& ref_base() {return (*this)[k_ref_base]; };
      uint32_t& obs_base() {return (*this)[k_obs_base]; };
      uint32_t& quality()  {return (*this)[k_quality]; };
      uint32_t& read_pos() {return (*this)[k_read_pos]; };
      uint32_t& base_repeat() {return (*this)[k_base_repeat]; };

      uint32_t read_set() const {return (*this)[k_read_set]; };
      uint32_t ref_base() const {return (*this)[k_ref_base]; };
      uint32_t obs_base() const {return (*this)[k_obs_base]; };
      uint32_t quality()  const {return (*this)[k_quality]; };
      uint32_t read_pos() const {return (*this)[k_read_pos]; };
      uint32_t base_repeat() const {return (*this)[k_base_repeat]; };

  };
  
 
 class cErrorTable {
    private:
      static const char m_sep;   //* separator between entries within a line in files
      static const string covariate_names[];
      
      std::vector<double> m_count_table;
      std::vector<double> m_log10_prob_table;

      typedef bool covariates_used_t[k_num_covariates];
      typedef uint32_t covariates_max_t[k_num_covariates];
      typedef bool covariates_enforce_max_t[k_num_covariates];
      typedef uint32_t covariates_offset_t[k_num_covariates];

    public:

      //* does not allocate table or assign covariates
      cErrorTable(): m_per_position(false) {};
      
      //* create table with covariates read from command line options
      cErrorTable(const string& colnames);
      
      //* create empty table
      cErrorTable(covariates_used_t covariate_used, covariates_max_t covariate_max, covariates_enforce_max_t covariate_enforce_max, bool per_position); // for creating empty table

      //* create summed sub-tables
      cErrorTable(cErrorTable& error_table, covariates_used_t covariates); 
      
      ~cErrorTable() {};

      //* Helper functions
      void allocate_table();
      uint32_t covariates_to_index(const covariate_values_t& cv);
      void index_to_covariates(const uint32_t idx, covariate_values_t& cv);

      void read_covariates(const std::string& colnames);
      std::string print_covariates();
      //void split(const std::string& s, char c, std::vector<std::string>& v); // helper function

      //* IO of tables
      void read_log10_prob_table(const string& filename);
      void write_log10_prob_table(const string& output_file);
      void write_base_qual_only_prob_table(const string& filename, const vector<string>& readfiles);
      void write_count_table(const string& out);
      void write_count_table_header(ofstream& out);
      void write_count_table_content(ofstream& out, const uint32_t position = 0);

      //* recording counts during error calibration     
      void count_alignment_position(const pileup_alignment& i, const pileup& p);
      void count_covariate(const covariate_values_t& cv);
      void counts_to_log10_prob();
   
      void clear() 
      { 
        for(vector<double>::iterator it=m_count_table.begin(); it != m_count_table.end(); it++ )
        {
          *it = 0;
        }
      }
      
      //* determining error probabilities to use during
      bool alignment_position_to_covariates(const pileup_alignment& a, int32_t insert_count, covariate_values_t& cv);
      
      //* accessors
      double get_log10_prob(covariate_values_t& cv);
   
      
    protected:
      covariates_used_t         m_covariate_used;         // list of covariates that are used by table
      covariates_max_t          m_covariate_max;          // maximum value of each covariate
      covariates_enforce_max_t  m_covariate_enforce_max;  // do not throw an error if max exceeded, reassign value to max
      covariates_offset_t       m_covariate_offset;       // number to multiply this covariate by when constructing row numbers

    public:
      bool                      m_per_position;

  };
	
	/*! Error-counting class.
	 
	 This class is used by the above error_count() function in order to count errors.
	 */
	class error_count_pileup : public breseq::pileup_base {
	public:
		
		//! Information that is tracked per-sequence.
		struct sequence_info {
			/*! Coverage count table.
			 
			 This is a table of non-deletion reads per position to non-redundancy counts.
			 For example, given unique_only_coverage[i] = x, for all aligned positions p:
			 i is the number of reads that do not indicate a deletion at p
			 x is the number of positions that have no redundancies
			 */
			std::vector<int> unique_only_coverage;
		};
		
		
		//! Constructor.
		error_count_pileup(const std::string& bam, 
                       const std::string& fasta,
                       const string& output_dir,
                       bool do_coverage, 
                       bool do_errors, 
                       uint8_t min_qual_score, 
                       const std::string& covariates
                       );
		
		//! Destructor.
		virtual ~error_count_pileup();		
		
		//! Called for each alignment.
		virtual void pileup_callback(const pileup& p);
		
		//! Print coverage distribution.
		void print_coverage();
		
		//! Print error file.
		void print_error(const std::vector<std::string>& readfiles);

	protected:		
    const string& m_output_dir;
		vector<sequence_info> _seq_info; //!< information about each sequence.
		bool m_do_coverage;
    bool m_do_errors;
    uint8_t m_min_qual_score; //! @JEB THIS IS CURRENTLY NOT USED (BUT WOULD BE IF WE CALCULATED RATES)
    cErrorTable m_error_table;
    ofstream m_per_position_file;
	};
	

  /*! New handler for creating
	 */
	class error_count_results {
	public:
		typedef std::map<std::string,std::pair<double,double> > base_error_t; // basepair -> (error,correct) rates
		typedef std::map<uint8_t,base_error_t> error_map_t;
		typedef std::vector<error_map_t> fastq_error_map_t;
		
		//! Constructor.
		error_count_results(const std::string& input_dir, const std::vector<std::string>& readfiles);
		
		//! Return the correct rate for the given base pair, quality, and FASTQ file index.
		double log10_correct_rates(int32_t fastq_file_index, uint8_t quality, const std::string& base_key);
		double correct_rates(int32_t fastq_file_index, uint8_t quality, const std::string& base_key);

		//! Return the error rate for the given base pair, quality, and FASTQ file index.
		double log10_error_rates(int32_t fastq_file_index, uint8_t quality, const std::string& base_key);

		//! Return the pair of (error,correct) rates.
		const std::pair<double,double>& log10_rates(int32_t fastq_file_index, uint8_t quality, const std::string& base_key);
		
	protected:
		fastq_error_map_t _log10_error_rates; //!< fastq_file_index -> quality -> error rate map.
		fastq_error_map_t _error_rates; //!< fastq_file_index -> quality -> error rate map.

	};
  
  
	
} // breseq

#endif
