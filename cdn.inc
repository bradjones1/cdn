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
  $page_stats = (bool) variable_get('cdn_dev_page_stats', 0);

  $file_path = cdn_clean_filepath($file_path);

  if (in_array($file_path, array_keys($files_synced))) {
    $remote_file_path = variable_get('cdn_url', base_path()) . $files_synced[$file_path];
    if ($page_stats) {
      _cdn_devel_page_stats($file_path, $remote_file_path, TRUE);
    }
    return $remote_file_path;
  }
  else {
    $local_file_path = base_path() . $file_path;
    if ($page_stats) {
      _cdn_devel_page_stats($file_path, $local_file_path, FALSE);
    }
    return $remote_local_path;
  }
}

/**
 * Alters filenames or filepaths to make the filename unique.
 *
 * @param $filepath
 *   The path to the file, relative to the Drupal root directory.
 * @param $unique_settings
 *   The settings to generate the unique filepath for the CDN. This is an
 *   array with the following structure:
 *   array(
 *     'method' => 'mtime',    // Other values: 'md5'.
 *     'where'  => 'filename', // Other values: 'common parent directory' (should be used for themes to not break URLs in CSS files, enables a new method: 'md5 of mtimes').
 *     'params' => array(),    // An array of parameters. Optional. Keys in case of 'common parent directory': 'path' and 'files'. 
 *   )
 * @return
 *   The new basename.
 */
function cdn_unique_filename($filepath, $unique_settings = array()) {
  // Generate the file version identifier that will be included in the
  // name to make sure we get a unique filename.
  switch ($unique_settings['method']) {
    case 'md5 of mtimes':
      static $md5_mtimes;

      if (!isset($md5_mtimes[$unique_settings['params']['path']])) {
        $mtimes_string = '';
        foreach ($unique_settings['params']['files'] as $file) {
          $mtimes_string .= filemtime($file); 
        }
        $md5_mtimes[$unique_settings['params']['path']] = md5($mtimes_string);
      }
      $unique = $md5_mtimes[$unique_settings['params']['path']];
      break;

    case 'md5':
      $unique = md5_file($filepath);
      break;

    case 'mtime':
    default:
      $unique = filemtime($filepath);
      break;
  }

  $dirs = explode('/', $filepath);
  $basename = end($dirs);
  unset($dirs[count($dirs) - 1]);
  $path = implode('/', $dirs);
  
  switch ($unique_settings['where']) {
    case 'filename':
      if (($pos = strrpos($basename, '.')) !== FALSE) {
        $first = substr($basename, 0, $pos);
        $last = substr($basename, $pos, strlen($basename) - $pos);
        $basename = $first .'-'. $unique . $last;
      }
      else {
        $basename .= '-'. $unique;
      }
      break;

    case 'common parent directory':
      $path = $dirs[0];
      for ($i = 1; $i < count($dirs); $i++) {
        $path .= "/$dirs[$i]";
        if ($path == $unique_settings['params']['path']) {
          $path .= "/$unique";
        }
      }   
      break;
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
