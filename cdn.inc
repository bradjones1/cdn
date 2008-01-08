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
    $remote_file_path = variable_get('cdn_url', 'yourcdn.com/') . $files_synced[$file_path];
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
    return $local_file_path;
  }
}

/**
 * Alters filename or file path to make the filename unique.
 *
 * @param $file_path
 *   The path to the file, relative to the Drupal root directory.
 * @param $unique_settings
 *   The settings to generate the unique file path for the CDN. This is an
 *   array with the following structure:
 *   array(
 *     'method' => 'mtime',    // Other values: 'md5'.
 *     'where'  => 'filename', // Other values: 'common parent directory' (should be used for themes to not break URLs in CSS files, enables a new method: 'md5 of mtimes').
 *     'params' => array(),    // An array of parameters. Optional. Keys in case of 'common parent directory': 'path' and 'files'. 
 *   )
 * @return
 *   The new basename.
 */
function cdn_unique_filename($file_path, $unique_settings = array()) {
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
      $unique = md5_file($file_path);
      break;

    case 'mtime':
    default:
      $unique = filemtime($file_path);
      break;
  }

  $dirs = explode('/', $file_path);
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
 * Removes the leading "/", "/" or "../"'s from a path.
 *
 * @param $path
 *   A path.
 * @return
 *   The cleaned path.
 */
function cdn_clean_filepath($path) {
  return preg_replace('/^(?:(?:\.\.\/)+|\.\/|\/)?(.*)/', '$1', $path);
}

/**
 * Updates the contents of the file for CDN compatibility: it searches all
 * relative URLs and replaces it with absolute URLs to files hosted on the
 * CDN, if they're available on the CDN already, otherwise they are replaced
 * with absolute URLs to the files hosted on the local server.
 *
 * @param $file_path
 *   The path to the file, relative to the Drupal root directory.
 * @return
 *   Path to the updated file (the original file will never be changed).
 */
function cdn_update_file($file_path) {
  $contents = file_get_contents($file_path);

  // Strip surrounding apostrophes or quotes.
  $contents = preg_replace('/(?<=url\()([\'"]{1})(.*)\1{1}/', '\2', $contents);
  
  // Remove the leading base path.
  $contents = preg_replace('/url\('. preg_quote(base_path(), '/.') .'([\d\w-_\.\/]+)/i', 'url(\1', $contents);

  // Strip all "../" and everything before it, ignoring absolute paths.
  $contents = preg_replace('/url\(([\'"]?)(?![a-z]+:)[\d\w-_\.\/]+(\.\.\/)+/i', 'url(\1', $contents);

  // Route all paths through cdn_file(), ignoring absolute paths.
  $contents = preg_replace_callback(
    '/(?<=url\()[\d\w-_\.\/]+(?=\))/i',
    create_function(
      '$matches',
      'return cdn_file($matches[0]);'
    ),
    $contents
  );

  // Create the CDN directory if it does not yet exist.
  file_check_directory(file_create_path('cdn'), FILE_CREATE_DIRECTORY);

  return file_save_data($contents, 'cdn/'. md5($file_path), FILE_EXISTS_REPLACE);
}
