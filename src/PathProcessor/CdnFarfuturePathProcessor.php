<?php

namespace Drupal\cdn\PathProcessor;

use Drupal\cdn\File\FileUrlGenerator;
use Drupal\Component\Utility\UrlHelper;
use Drupal\Core\PathProcessor\InboundPathProcessorInterface;
use Symfony\Component\HttpFoundation\Request;

/**
 * Defines a path processor to rewrite CDN farfuture URLs.
 *
 * As the route system does not allow arbitrary amount of parameters convert
 * the file path to a query parameter on the request.
 *
 * Also normalizes legacy far-future URLs generated prior to
 * https://www.drupal.org/node/2870435
 *
 * @see \Drupal\image\PathProcessor\PathProcessorImageStyles
 */
class CdnFarfuturePathProcessor implements InboundPathProcessorInterface {

  /**
   * {@inheritdoc}
   */
  public function processInbound($path, Request $request) {
    if (!preg_match('/^\/cdn\/(ff|farfuture)\/.*/', $path, $matches)) {
      return $path;
    }

    // Parse the security token, mtime and root-relative file URL.
    // Backwards compatibility for non-scheme aware farfuture paths.
    if ($matches[1] == 'farfuture') {
      // Normalize legacy path.
      // Parse the security token, mtime and root-relative file URL.
      $tail = substr($path, strlen('/cdn/farfuture/'));
      list($security_token, $mtime, $root_relative_file_url) = explode('/', $tail, 3);
      $returnPath = "/cdn/ff/$security_token/$mtime/" . FileUrlGenerator::RELATIVE;
    }
    else {
      // Parse the security token, mtime, scheme and root-relative file URL.
      $tail = substr($path, strlen('/cdn/ff/'));
      list($security_token, $mtime, $scheme, $root_relative_file_url) = explode('/', $tail, 4);
      $returnPath = "/cdn/ff/$security_token/$mtime/$scheme";
    }
    // Set the root-relative file URL as query parameter.
    $request->query->set('root_relative_file_url', '/' . UrlHelper::encodePath($root_relative_file_url));

    // Return the same path, but without the trailing file.
    return $returnPath;
  }

}
