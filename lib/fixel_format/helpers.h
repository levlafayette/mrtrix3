/*
 * Copyright (c) 2008-2016 the MRtrix3 contributors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/
 *
 * MRtrix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * For more details, see www.mrtrix.org
 *
 */

#ifndef __fixel_helpers_h__
#define __fixel_helpers_h__

#include "formats/mrtrix_utils.h"
#include "fixel_format/keys.h"

namespace MR
{
  class InvalidFixelDirectoryException : public Exception
  {
    public:
      InvalidFixelDirectoryException (const std::string& msg) : Exception(msg) {}
      InvalidFixelDirectoryException (const Exception& previous_exception, const std::string& msg)
        : Exception(previous_exception, msg) {}
  };

  namespace FixelFormat
  {
    inline bool is_index_image (const Header& in)
    {
      return in.keyval ().count (n_fixels_key);
    }


    inline void check_index_image (const Header& in)
    {
      if (!is_index_image (in))
        throw InvalidImageException (in.name () + " is not a valid fixel index image. Header key " + n_fixels_key + " not found");
    }


    inline bool is_data_file (const Header& in)
    {
      return in.ndim () == 3 && in.size (2) == 1;
    }


    inline bool is_directions_file (const Header& in)
    {
      std::string basename (Path::basename (in.name()));
      return in.ndim () == 3 && in.size (1) == 3 && in.size (2) == 1 && (basename.substr(0, basename.find_last_of(".")) == "directions");
    }


    inline void check_data_file (const Header& in)
    {
      if (!is_data_file (in))
        throw InvalidImageException (in.name () + " is not a valid fixel data file. Expected a 3-dimensional image of size n x m x 1");
    }

    inline std::string get_fixel_folder (const std::string& fixel_file) {
      std::string fixel_folder = Path::dirname (fixel_file);
      // assume the user is running the command from within the fixel directory
      if (fixel_folder.empty())
        fixel_folder = Path::cwd();
      return fixel_folder;
    }

    template <class IndexHeaderType, class DataHeaderType>
    inline bool fixels_match (const IndexHeaderType& index_header, const DataHeaderType& data_header)
    {
      bool fixels_match (false);

      if (is_index_image (index_header))
        fixels_match = std::stoul(index_header.keyval ().at (n_fixels_key)) == (unsigned long)data_header.size (0);

      return fixels_match;
    }


    inline void check_fixel_size (const Header& index_h, const Header& data_h)
    {
      check_index_image (index_h);
      check_data_file (data_h);

      if (!fixels_match (index_h, data_h))
        throw InvalidImageException ("Fixel number mismatch between index image " + index_h.name() + " and data image " + data_h.name());
    }


    inline void check_fixel_folder (const std::string &path, bool create_if_missing = false, bool check_if_empty = false)
    {
      std::string path_temp = path;
      // the user must be inside the fixel folder
      if (path.empty())
        path_temp = Path::cwd();

      bool exists (true);

      if (!(exists = Path::exists (path_temp))) {
        if (create_if_missing) File::mkdir (path_temp);
        else throw Exception ("Fixel directory (" + str(path_temp) + ") does not exist");
      }
      else if (!Path::is_dir (path_temp))
        throw Exception (str(path_temp) + " is not a directory");

      if (check_if_empty && Path::Dir (path_temp).read_name ().size () != 0)
        throw Exception ("Expected fixel directory " + path_temp + " to be empty.");
    }


    inline Header find_index_header (const std::string &fixel_folder_path)
    {
      bool index_found (false);
      Header H;
      check_fixel_folder (fixel_folder_path);

      auto dir_walker = Path::Dir (fixel_folder_path);
      std::string fname;
      while ((fname = dir_walker.read_name ()).size ()) {
        auto full_path = Path::join (fixel_folder_path, fname);
        if (Path::has_suffix (fname, FixelFormat::supported_fixel_formats) && is_index_image (H = Header::open (full_path))) {
          index_found = true;
          break;
        }
      }

      if (!index_found)
        throw InvalidFixelDirectoryException ("Could not find index image in directory " + fixel_folder_path);

      return H;
    }


    inline std::vector<Header> find_data_headers (const std::string &fixel_folder_path, const Header &index_header, const bool include_directions = false)
    {
      check_index_image (index_header);

      std::vector<Header> data_headers;

      auto dir_walker = Path::Dir (fixel_folder_path);
      std::string fname;
      while ((fname = dir_walker.read_name ()).size ()) {
        auto full_path = Path::join (fixel_folder_path, fname);
        Header H;
        if (Path::has_suffix (fname, FixelFormat::supported_fixel_formats) && is_data_file (H = Header::open (full_path))) {
          if (fixels_match (index_header, H)) {
            if (!is_directions_file (H) || include_directions)
              data_headers.emplace_back (std::move (H));
          } else {
            WARN ("fixel data file (" + fname + ") does not contain the same number of elements as fixels in the index file" );
          }
        }
      }

      return data_headers;
    }

    template <class IndexHeaderType>
    inline Header find_directions_header (const std::string &fixel_folder_path, const IndexHeaderType &index_header)
    {

      bool directions_found (false);
      Header header;
      check_fixel_folder (fixel_folder_path);

      auto dir_walker = Path::Dir (fixel_folder_path);
      std::string fname;
      while ((fname = dir_walker.read_name ()).size ()) {
        Header tmp_header;
        auto full_path = Path::join (fixel_folder_path, fname);
        if (Path::has_suffix (fname, FixelFormat::supported_fixel_formats)
              && is_directions_file (tmp_header = Header::open (full_path))) {
          if (is_directions_file (tmp_header)) {
            if (fixels_match (index_header, tmp_header)) {
              if (directions_found == true)
                throw Exception ("multiple directions files found in fixel image folder: " + fixel_folder_path);
              directions_found = true;
              header = std::move(tmp_header);
            } else {
              WARN ("fixel directions file (" + fname + ") does not contain the same number of elements as fixels in the index file" );
            }
          }
        }
      }

      if (!directions_found)
        throw InvalidFixelDirectoryException ("Could not find directions image in directory " + fixel_folder_path);

      return header;
    }


    //! Copy a file from one fixel folder into another.
    inline void copy_fixel_file (const std::string& input_file_path, const std::string &output_folder) {
      check_fixel_folder (output_folder, true);
      std::string output_path = Path::join (output_folder, Path::basename (input_file_path));
      Header input_header = Header::open (input_file_path);
      auto input_image = input_header.get_image<float>();
      auto output_image = Image<float>::create (output_path, input_header);
      threaded_copy (input_image, output_image);
    }

    //! Copy the index file from one fixel folder into another
    inline void copy_index_file (const std::string &input_folder, const std::string &output_folder) {
      Header input_header = FixelFormat::find_index_header (input_folder);
      check_fixel_folder (output_folder, true);
      auto output_image = Image<uint32_t>::create (Path::join (output_folder, Path::basename (input_header.name())), input_header);
      auto input_image = input_header.get_image<uint32_t>();
      threaded_copy (input_image, output_image);
    }

    //! Copy the directions file from one fixel folder into another.
    inline void copy_directions_file (const std::string &input_folder, const std::string &output_folder) {
      Header input_header = FixelFormat::find_directions_header (input_folder, FixelFormat::find_index_header (input_folder));
      copy_fixel_file (input_header.name(), output_folder);
    }


    //! Copy all data files in a fixel folder into another folder. Data files do not include the index or directions file.
    inline void copy_all_data_files (const std::string &input_folder, const std::string &output_folder) {
      for (auto& input_header : FixelFormat::find_data_headers (input_folder, FixelFormat::find_index_header (input_folder)))
        copy_fixel_file (input_header.name(), output_folder);
    }

    //! open a data file. checks that a user has not input a fixel folder or index image
    template <class ValueType>
    Image<ValueType> open_fixel_data_file (const std::string& input_file) {
      if (Path::is_dir (input_file))
        throw Exception ("please input the specific fixel data file to be converted (not the fixel folder)");

      Header in_data_header = Header::open (input_file);
      FixelFormat::check_data_file (in_data_header);
      auto in_data_image = in_data_header.get_image<ValueType>();

      Header in_index_header = FixelFormat::find_index_header (FixelFormat::get_fixel_folder (input_file));
      if (input_file == in_index_header.name())
        throw Exception ("input fixel data file cannot be the index file");

      return in_data_image;
    }


  }
}

#endif

