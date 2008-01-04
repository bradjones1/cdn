<?php
// $Id$

/**
 * @file
 * Basic functions for CDN integration and synchronization.
 */


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
