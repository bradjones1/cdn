<?php
// $Id$

/**
 * @file
 * The cdn module allows you to
 */

/**
 * @return
 *   The new basename.
 */
function _cdn_timed_filename($filepath, $basename) {
  $mtime = filemtime($filepath);

  if (($pos = strrpos($basename, '.')) !== FALSE) {
    $first = substr($basename, 0, $pos);
    $last = substr($basename, $pos, strlen($basename) - $pos);
    $basename = $first .'-'. $mtime . $last;
  }
  else {
    $basename .= '-'. $mtime;
  }
  return $basename;  
}