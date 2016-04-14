<?php

namespace Drupal\cdn\File;

use Drupal\Component\Utility\Unicode;
use Drupal\Core\Config\ConfigFactoryInterface;
use Drupal\Core\Config\ConfigValueException;
use Drupal\Core\File\FileSystemInterface;
use Drupal\Core\StreamWrapper\StreamWrapperInterface;
use Drupal\Core\StreamWrapper\StreamWrapperManagerInterface;
use Symfony\Component\HttpFoundation\RequestStack;

/**
 * Generates CDN file URLs.
 *
 * @see https://www.drupal.org/node/2669074
 */
class FileUrlGenerator {

  /**
   * The file system service.
   *
   * @var \Drupal\Core\File\FileSystemInterface
   */
  protected $fileSystem;

  /**
   * The stream wrapper manager.
   *
   * @var \Drupal\Core\StreamWrapper\StreamWrapperManagerInterface
   */
  protected $streamWrapperManager;

  /**
   * The request stack.
   *
   * @var \Symfony\Component\HttpFoundation\RequestStack
   */
  protected $requestStack;

  /**
   * The CDN settings.
   *
   * @var \Drupal\Core\Config\ImmutableConfig
   */
  protected $settings;

  /**
   * The lookup table.
   *
   * @var array
   */
  protected $lookupTable;

  /**
   * Constructs a new CDN file URL generator object.
   *
   * @param \Drupal\Core\File\FileSystemInterface
   *   The file system service.
   * @param \Drupal\Core\StreamWrapper\StreamWrapperManagerInterface
   *   The stream wrapper manager.
   * @param \Symfony\Component\HttpFoundation\RequestStack $request_stack
   *   The request stack.
   * @param \Drupal\Core\Config\ConfigFactoryInterface $config_factory
   *   The config factory.
   */
  public function __construct(FileSystemInterface $file_system, StreamWrapperManagerInterface $stream_wrapper_manager, RequestStack $request_stack, ConfigFactoryInterface $config_factory) {
    $this->fileSystem = $file_system;
    $this->streamWrapperManager = $stream_wrapper_manager;
    $this->requestStack = $request_stack;
    $this->settings = $config_factory->get('cdn.settings');
    $this->lookupTable = $this->buildLookupTable($this->settings->get('mapping'));
  }

  /**
   * Generates a CDN file URL for local files that are mapped to a CDN.
   *
   * Compatibility: normal paths and stream wrappers.
   *
   * There are two kinds of local files:
   * - "managed files", i.e. those stored by a Drupal-compatible stream wrapper.
   *   These are files that have either been uploaded by users or were generated
   *   automatically (for example through CSS aggregation).
   * - "shipped files", i.e. those outside of the files directory, which ship as
   *   part of Drupal core or contributed modules or themes.
   *
   * @param string $uri
   *   The URI to a file for which we need a CDN URL, or the path to a shipped
   *   file.
   *
   * @return string|FALSE
   *   A string containing the protocol-relative CDN file URI, or FALSE if this
   *   file URI should not be served from a CDN.
   */
  public function generate($uri) {
    $status = $this->settings->get('status');
    if ($status === 0) {
      return FALSE;
    }
    // @todo testing mode, but testing mode's permission only lives in the CDN UI module…

    $root_relative_url = $this->getRootRelativeUrl($uri);
    if ($root_relative_url === FALSE) {
      return FALSE;
    }

    // Extension-specific mapping.
    $file_extension = Unicode::strtolower(pathinfo($uri, PATHINFO_EXTENSION));
    if (isset($this->lookupTable[$file_extension])) {
      $key = $file_extension;
    }
    // Generic or fallback mapping.
    elseif (isset($this->lookupTable['*'])) {
      $key = '*';
    }
    // No mapping.
    else {
      return FALSE;
    }

    $result = $this->lookupTable[$key];

    // If there are multiple results, pick one using consistent hashing: ensure
    // the same file is always served from the same CDN domain.
    if (is_array($result)) {
      $filename = basename($uri);
      $hash = hexdec(substr(md5($filename), 0, 5));
      $cdn_domain = $result[$hash % count($result)];
    }
    else {
      $cdn_domain = $result;
    }

    return '//' . $cdn_domain . $root_relative_url;
  }

  /**
   * Gets the root-relative URL for files that are shipped or in a local stream.
   *
   * @param string $uri
   *   The URI to a file for which we need a CDN URL, or the path to a shipped
   *   file.
   *
   * @return bool|string
   *   Returns FALSE if the URI is not for a shipped file or in a local stream.
   *   Otherwise, returns the root-relative URL.
   */
  protected function getRootRelativeUrl($uri) {
    $scheme = $this->fileSystem->uriScheme($uri);

    // If the URI is absolute — HTTP(S) or otherwise — return early, except if
    // it's an absolute URI using a local stream wrapper scheme.
    if ($scheme && !isset($this->streamWrapperManager->getWrappers(StreamWrapperInterface::LOCAL)[$scheme])) {
      return FALSE;
    }
    // If the URI is protocol-relative, return early.
    elseif (Unicode::substr($uri, 0, 2) === '//') {
      return FALSE;
    }

    $request = $this->requestStack->getCurrentRequest();

    return $scheme
      // Local stream wrapper.
      ? str_replace($request->getSchemeAndHttpHost(), '', $this->streamWrapperManager->getViaUri($uri)->getExternalUrl())
      // Shipped file.
      : $request->getBasePath() . '/' . $uri;
  }

  /**
   * Builds a lookup table: file extension to CDN domain(s).
   *
   * @param array $mapping
   *   An array matching either of the mappings in cdn.mapping.schema.yml.
   *
   * @return array
   *   A lookup table. Keys are lowercase file extensions or the asterisk.
   *   Values are CDN domains (either string if only one, or array of strings if
   *   multiple).
   *
   * @throws \Drupal\Core\Config\ConfigValueException
   *
   * @todo Abstract this out further in the future if the need arises, i.e. if
   *       more conditions besides extensions are added. For now, KISS.
   */
  protected function buildLookupTable(array $mapping) {
    $lookup_table = [];
    if ($mapping['type'] === 'simple') {
      $domain = $mapping['domain'];
      assert('strpos($domain, "/") === FALSE && strpos($domain, ":") === FALSE', "The provided domain $domain is not a valid domain. Provide domains or hostnames of the form 'cdn.com', 'cdn.example.com'. IP addresses and ports are also allowed.");
      if (empty($mapping['conditions'])) {
        $lookup_table['*'] = $domain;
      }
      else {
        if (empty($mapping['conditions']['extensions'])) {
          $lookup_table['*'] = $domain;
        }
        else {
          foreach ($mapping['conditions']['extensions'] as $extension) {
            $lookup_table[$extension] = $domain;
          }
        }
      }
    }
    elseif ($mapping['type'] === 'complex') {
      $fallback_domain = NULL;
      if (isset($mapping['fallback_domain'])) {
        $fallback_domain = $mapping['fallback_domain'];
        assert('strpos($fallback_domain, "/") === FALSE && strpos($fallback_domain, ":") === FALSE', "The provided fallback domain $fallback_domain is not a valid domain. Provide domains or hostnames of the form 'cdn.com', 'cdn.example.com'. IP addresses and ports are also allowed.");
        $lookup_table['*'] = $fallback_domain;
      }
      foreach ($mapping['domains'] as $nested_mapping) {
        $lookup_table += $this->buildLookupTable($nested_mapping);
      }
    }
    elseif ($mapping['type'] === 'auto-balanced') {
      if (empty($mapping['conditions']) || empty($mapping['conditions']['extensions'])) {
        throw new ConfigValueException('It does not make sense to apply auto-balancing to all files, regardless of extension.');
      }
      $domains = $mapping['domains'];
      foreach ($domains as $domain) {
        assert('strpos($domain, "/") === FALSE && strpos($domain, ":") === FALSE', "The provided domain $domain is not a valid domain. Provide domains or hostnames of the form 'cdn.com', 'cdn.example.com'. IP addresses and ports are also allowed.");
      }
      foreach ($mapping['conditions']['extensions'] as $extension) {
        $lookup_table[$extension] = $domains;
      }
    }
    else {
      throw new ConfigValueException('Unknown CDN mapping type specified.');
    }
    return $lookup_table;
  }

}
