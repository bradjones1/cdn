// $Id$

Description
-----------
The aim of this module to provide easy Content Delivery Network integration
for Drupal sites. Obviously it has to patch Drupal core to rewrite the URLs,
not only to serve them from another domain, but also to make the filenames
unique.

It has synchronization plugins, so it allows you to use any protocol or
algorithm to synchronize your files. Currently however, only one plugin is
available: FTP. Since proper usage of a CDN demands unique filenames for each
version of a file, we can optimize a lot: to validate a file on the CDN while
synchronizing, we must only know if it 1) exists and 2) has the correct size.

Which files and directories should be synchronized can be configured very
precisely. Consult the README for details about that.

The FTP synchronization plugin allows you to use a $15 per month CDN (thus
making CDNs accessible to /a lot/ of Drupal users) with no effort after the
installation!
For those who know of the infamous YSlow test: if you install and configure
this module and apply the core patch that also adds Javascript aggregation,
you will score 98. Almost the maximum! The remainder of points is due to the
lack Javascript minification (compression).

This module was developed for http://driverpacks.net/.


Aren't CDN's so expensive only big companies can afford them?
-------------------------------------------------------------
Not anymore (in order of best price-value ratio):

1) CacheFly, http://cachefly.com/, starts at USD 15 for 30 GB per month

2) Influxis, http://influxis.com/, starts at USD 10 for 1 GB per month


Installation
------------
1) Place this module in your modules directory (this will usually be
"sites/all/modules/").

2) Enable the module.

3) Apply the Drupal core patch. See below.

4) Apply the theme patch to every theme. See below.

5) Read how to configure the CDN synchronization filters. See below.

6) Configure the $conf array in settings.php See below.

7) Copy cdn_cron.php to your Drupal root directory.

8) Configure cdn_cron.php like Drupal's cron.php. See http://drupal.org/cron.

9) Go to admin/logs/status. If the CDN integration module is installed
correctly or not, it will report so here.


Usage
-----
When the module is installed properly (see step 9 of the installation), you
can check the site-wide statistics at admin/settings/cdn. At that same page,
you can enable the per-page statistics as well. This will show the number of
files served from the CDN at the bottom of each page, as well as a list of
files that haven't been synchronized to the CDN yet, to users with the
"administer site configuration" permission.


Applying the Drupal core patch
------------------------------
You *must* apply this patch! It has been created against Drupal 5.5.

First, change the directory to the Drupal root directory.

You can apply the included Drupal core patch like this:
  patch -p0 < drupal_core_cdn_integration.patch

To undo the patch:
  patch -p0 -R < drupal_core_cdn_integration.patch

Note: there is also a patch that combines the CDN integration core patch with
the JS aggregation. It's included in this module because if you apply both
patches separately, you will get a conflict.


Applying the theme patch
------------------------
You *must* apply this patch to *every* theme that's being used on your website!

Repeat this process for every theme: first, change the directory to the
directory of the theme. Applying the patch is identical to the example above,
only with a different filename.


Setting up CDN synchronization filters
--------------------------------------
First of all: each filter works *recursively*! Now, the explanations:
- paths: This is an array of paths (each path being relative to the Drupal
         root directory) on which this filter should be applied.
- pattern: Regular expression that will be used to filter the files in each
           directory. Like the $mask parameter in file_scan_directory().
- ignored_dirs: Array of directories that should be ignored in each directory.
                Like the $nomask parameter in file_scan_directory().
- unique: Determines how the uniqueness will be applied. You can set it to
          'filename', which will alter the filename, or 'common parent
          directory', which will alter the path of the file. The latter is
          strongly recommended for themes, since it will not break URLs in
          CSS files.
- unique_method: The method that should be used to generate unique filenames.
                 Currently supported: 'none' (no unique filename!), 'mtime'
                 (the file's mtime property), 'md5' (md5 hash of the file) or
                 'md5 of mtimes' (md5 hash of the concatenated mtimes of a set
                 of files). This last option is only available if you have set
                 the unique property to 'common parent directory'.


Configuring the $conf array in settings.php
-------------------------------------------
This is my configuration:

$conf = array(
  'file_url_rewrite' => array(
    'cdn_file_url', // List the CDN module's URL rewrite function as the preferred server.
  ),
  'cdn_url' => 'http://wimleers.cachefly.com/wimleers.com',
  'cdn_sync_filters' => array(
    // Add all Javascript, CSS, image and flash files from the most common
    // directories in Drupal.
    0 => array(
      'paths' => array('misc', 'profiles', 'modules', 'sites/all/modules', 'sites/default/modules'),
      'pattern' => '.*\.(ico|js|css|gif|png|jpg|jpeg|svg|swf)$',
      'ignored_dirs' => array('CVS'),
      'unique' => 'filename',
      'unique_method' => 'mtime',
    ),

    // We want to add *everything* in the files directory. Except for the
    // files in the CSS and JS directory, because they need other treatment:
    // we assume that files in this directory don't change, so we can use
    // non-unique filenames, resulting in nicer filenames when they're
    // downloaded.
    1 => array(
      'paths' => array('sites/wimleers.com/files'),
      'pattern' => '.*',
      'ignored_dirs' => array('CVS', 'css', 'js'),
      'unique_method' => 'none',
    ),

    // Add all files in the files/css directory, *but* update the URLs in the
    // files. This is only necessary if we use CSS aggregation.
    2 => array(
      'paths' => array('sites/wimleers.com/files/css'),
      'pattern' => '.*',
      'ignored_dirs' => array('CVS'),
      'unique' => 'filename',
      'unique_method' => 'mtime',
      'update_urls_in_files' => TRUE,
    ),

    // Add all files in the files/js directory. This is only necessary if we
    // use JS aggregation.
    3 => array(
      'paths' => array('sites/wimleers.com/files/js'),
      'pattern' => '.*',
      'ignored_dirs' => array('CVS'),
      'unique' => 'filename',
      'unique_method' => 'mtime',
    ),

    // Add all Javascript, CSS, image and font files from our themes. But
    // make sure the URLs don't break when CSS aggregation is disabled, by
    // using the "common parent directory" unique level and the "md5 of mtimes"
    // uniqueness method. We can revert to normal values if we have CSS
    // aggregation enabled.
    4 => array(
      'paths' => array('sites/default/themes/garland-customized'),
      'pattern' => '.*\.(js|css|gif|png|jpg|jpeg|otf)$', // We *include* css files, because some (e.g. fix-ie.css) are not included in the aggregation.
      'ignored_dirs' => array('CVS'),
      'unique' => 'common parent directory',
      'unique_method' => 'md5 of mtimes',
    ),
  ),
  'cdn_sync_method' => 'ftp',
  'cdn_sync_method_settings' => array(
    'host' => 'ftp.cachefly.com',
    'remote_path' => 'wimleers.com',
    'port' => 21,
    'user' => 'user',
    'pass' => 'pass',
  ),
);


Author
------
Wim Leers

* mail: work@wimleers.com
* website: http://wimleers.com/work

The author can be contacted for paid customizations of this module as well as
Drupal consulting, development and installation.
