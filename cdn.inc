<?php
// $Id$

/**
 * @file
 * Basic functions for CDN integration and synchronization.
 */

/**
 * Given a file path relative to the Drupal root directory, get the URL of the
 * corresponding file on the CDN, or the normal URL if the corresponding file
 * does not (yet) exist on the CDN.
 *
 * @param $file_path
 *   A file path relative to the Drupal root directory.
 * @return
 *   The URL to the file on the CDN, if available, otherwise the normal URL.
 */
function cdn_file($file_path) {
  $files_synced = variable_get('cdn_files_synced', array());

  $file_path = cdn_clean_filepath($file_path);

  if (in_array($file_path, array_keys($files_synced))) {
    $remote_file_path = variable_get('cdn_url', base_path()) . $files_synced[$file_path];
    _cdn_devel_page_stats($file_path, $remote_file_path, TRUE);
    return $remote_file_path;
  }
  else {
    $local_file_path = base_path() . $file_path;
    _cdn_devel_page_stats($file_path, $local_file_path, FALSE);
    return $remote_local_path;
  }
}

/**
 * Alters filenames or filepaths to make the filename unique.
 *
 * @param $filepath
 *   The path to the file, relative to the Drupal root directory.
 * @param $file
 *   Either a file name or the path to a file. If FALSE, then $file will be
 *   set to $filepath.
 * @return
 *   The new basename.
 */
function cdn_unique_filename($filepath, $file = FALSE) {
  // Generate the file version identifier that will be included in the
  // name to make sure we get a unique filename.
  switch (variable_get('cdn_unique_filenames_method', 'mtime')) {
    case 'md5':
      $unique = md5_file($filepath);
      break;

    case 'mtime':
    default:
      $unique = filemtime($filepath);
      break;
  }

  if (!$file) {
    $file = $filepath;
  }

  // Include the version identifier.
  $dirs = explode('/', $file);
  $basename = end($dirs);
  unset($dirs[count($dirs) - 1]);
  $path = implode('/', $dirs);
  
  if (($pos = strrpos($basename, '.')) !== FALSE) {
    $first = substr($basename, 0, $pos);
    $last = substr($basename, $pos, strlen($basename) - $pos);
    $basename = $first .'-'. $unique . $last;
  }
  else {
    $basename .= '-'. $unique;
  }

  return $path .'/'. $basename;
}

/**
 * Removes the leading "./" from a path. Designed to be used with array_map().
 *
 * @param $path
 *   A path.
 * @return
 *   The path without the leading "./".
 */
function cdn_clean_filepath($path) {
  return preg_replace('/^\.?\/?(.*)/', '$1', $path);
}

/**
 * Collects per-page CDN integration statistics.
 *
 * @param $file
 *   The local file path.
 * @param $remote_file
 *   The remote file path.
 * @param $remote_file_exists
 *   TRUE if the remote file exists, FALSE otherwise.
 * @return
 *   Only if no parameters were passed: the collected statistics.
 */
function _cdn_devel_page_stats($file = FALSE, $remote_file = '', $remote_file_exists = FALSE) {
  static $file_count, $remote_file_count, $local_files;

  if (!isset($local_files)) {
    $local_files = array();
  }

  // If the function is called with parameters set, save the statistics. If no
  // parameters are passed, return the collected statistics.
  if ($file) {
    
    $file_count++;

    if ($remote_file_exists) {
      $remote_file_count++;
    }
    else {
      $local_files[] = $file;
    }
  }
  else {
    return array(
      'file_count' => $file_count,
      'remote_file_count' => $remote_file_count,
      'local_files' => $local_files,
    );
  }
}
