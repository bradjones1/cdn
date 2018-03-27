<?php

namespace Drupal\cdn;

use Drupal\Component\Utility\Unicode;
use Drupal\Core\Config\ConfigFactoryInterface;
use Drupal\Core\Config\ConfigValueException;
use Drupal\Core\StreamWrapper\StreamWrapperInterface;
use Drupal\Core\StreamWrapper\StreamWrapperManagerInterface;

/**
 * Wraps the CDN settings configuration, contains all parsing.
 *
 * @internal
 */
class CdnSettings {

  /**
   * The CDN settings.
   *
   * @var \Drupal\Core\Config\ImmutableConfig
   */
  protected $rawSettings;

  /**
   * The lookup table.
   *
   * @var array|null
   */
  protected $lookupTable;

  /**
   * The stream wrapper manager.
   *
   * @var \Drupal\Core\StreamWrapper\StreamWrapperManagerInterface
   */
  protected $streamWrapperManager;

  /**
   * Constructs a new CdnSettings object.
   *
   * @param \Drupal\Core\Config\ConfigFactoryInterface $config_factory
   *   The config factory.
   */
  public function __construct(ConfigFactoryInterface $config_factory, StreamWrapperManagerInterface $streamWrapperManager) {
    $this->rawSettings = $config_factory->get('cdn.settings');
    $this->lookupTable = NULL;
    $this->streamWrapperManager = $streamWrapperManager;
  }

  /**
   * @return bool
   */
  public function isEnabled() {
    return $this->rawSettings->get('status') === TRUE;
  }

  /**
   * @return bool
   */
  public function farfutureIsEnabled() {
    return $this->rawSettings->get('farfuture.status') === TRUE;
  }

  /**
   * Returns the lookup table.
   *
   * @return array
   *   A lookup table. Keys are lowercase file extensions or the asterisk.
   *   Values are CDN domains (either string if only one, or array of strings if
   *   multiple).
   */
  public function getLookupTable() {
    if ($this->lookupTable === NULL) {
      $this->lookupTable = $this->buildLookupTable($this->rawSettings->get('mapping'));
    }
    return $this->lookupTable;
  }

  /**
   * Returns all unique CDN domains that are configured.
   *
   * @return string[]
   */
  public function getDomains() {
    $flattened = iterator_to_array(new \RecursiveIteratorIterator(new \RecursiveArrayIterator($this->getLookupTable())), FALSE);
    $unique_domains = array_unique(array_filter($flattened));
    return $unique_domains;
  }

  /**
   * Returns configured stream wrappers to apply CDN to.
   *
   * @return array
   */
  public function streamWrappers() {
    $configured = $this->rawSettings->get('stream_wrappers');
    // LOCAL_NORMAL stream wrappers are always served.
    return array_merge(array_keys($this->streamWrapperManager
      ->getWrappers(StreamWrapperInterface::LOCAL_NORMAL)), $configured);
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
      assert(CdnSettings::isValidCdnDomain($domain), "The provided domain $domain is not valid. Provide a host like 'cdn.com' or 'cdn.example.com'. IP addresses and ports are also allowed.");
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

        if (isset($mapping['conditions']['not'])) {
          assert(!isset($mapping['conditions']['extensions']), 'It does not make sense to provide an \'extensions\' condition as well as a negated \'extensions\' condition.');
          if (!empty($mapping['conditions']['not']['extensions'])) {
            foreach ($mapping['conditions']['not']['extensions'] as $not_extension) {
              $lookup_table[$not_extension] = FALSE;
            }
          }
        }
      }
    }
    elseif ($mapping['type'] === 'complex') {
      $fallback_domain = NULL;
      if (isset($mapping['fallback_domain'])) {
        $fallback_domain = $mapping['fallback_domain'];
        assert(CdnSettings::isValidCdnDomain($fallback_domain), "The provided fallback domain $fallback_domain is not valid. Provide a host like 'cdn.com' or 'cdn.example.com'. IP addresses and ports are also allowed.");
        $lookup_table['*'] = $fallback_domain;
      }
      for ($i = 0; $i < count($mapping['domains']); $i++) {
        $nested_mapping = $mapping['domains'][$i];
        assert(!empty($nested_mapping['conditions']), 'The nested mapping ' . $i . ' includes no conditions, which is not allowed for complex mappings.');
        assert(!isset($nested_mapping['conditions']['not']), 'The nested mapping ' . $i . ' includes negated conditions, which is not allowed for complex mappings: the fallback_domain already serves this purpose.');
        $lookup_table += $this->buildLookupTable($nested_mapping);
      }
    }
    elseif ($mapping['type'] === 'auto-balanced') {
      if (empty($mapping['conditions']) || empty($mapping['conditions']['extensions'])) {
        throw new ConfigValueException('It does not make sense to apply auto-balancing to all files, regardless of extension.');
      }
      $domains = $mapping['domains'];
      foreach ($domains as $domain) {
        assert(CdnSettings::isValidCdnDomain($domain), "The provided domain $domain is not valid. Provide a host like 'cdn.com' or 'cdn.example.com'. IP addresses and ports are also allowed.");
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

  /**
   * Validates the given CDN domain.
   *
   * @param string $domain
   *   A domain as expected by the CDN module. In fact, an "authority" as
   *   defined in RFC3986. An authority consists of host, optional userinfo and
   *   optional port. The host can be an IP address or registered domain name.
   *
   * @return bool
   *
   * @see https://tools.ietf.org/html/rfc3986#section-3.2
   * @see ../config/schema/cdn.data_types.schema.yml
   */
  public static function isValidCdnDomain($domain) {
    // Add a scheme so that we have a parseable URL.
    $url = 'https://' . $domain;
    $components = parse_url($url);

    $forbidden_components = ['path', 'query', 'fragment'];
    return $components === FALSE ? FALSE : empty(array_intersect($forbidden_components, array_keys($components)));
  }

  /**
   * Maps a URI to a CDN domain.
   *
   * @param string $uri
   *   The URI to map.
   *
   * @return string|bool
   *   The mapped domain, or FALSE if it could not be matched.
   */
  public function getCdnDomain($uri) {
    // Extension-specific mapping.
    $file_extension = Unicode::strtolower(pathinfo($uri, PATHINFO_EXTENSION));
    $lookup_table = $this->getLookupTable();
    if (isset($lookup_table[$file_extension])) {
      $key = $file_extension;
    }
    // Generic or fallback mapping.
    elseif (isset($lookup_table['*'])) {
      $key = '*';
    }
    // No mapping.
    else {
      return FALSE;
    }

    $result = $lookup_table[$key];

    if ($result === FALSE) {
      return FALSE;
    }
    // If there are multiple results, pick one using consistent hashing: ensure
    // the same file is always served from the same CDN domain.
    elseif (is_array($result)) {
      $filename = basename($uri);
      $hash = hexdec(substr(md5($filename), 0, 5));
      $cdn_domain = $result[$hash % count($result)];
    }
    else {
      $cdn_domain = $result;
    }
    return $cdn_domain;
  }
}
