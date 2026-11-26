# Desktop GPU proxies

With proxy clips enabled and the proxy profile set to Automatic, a compatible
NVIDIA source should use hardware decode, resize and encode when available.
If that attempt fails, the job should retry with software decode and resize
and NVIDIA encoding, then use software H.264 encoding if needed. All attempts
must write to the same proxy path, honor the configured width, keep audio and
stream mapping, and publish the proxy only after a successful encode. A canceled
job should stop and discard its partial file. Explicit proxy profiles and
camera-supplied proxies should retain their existing behavior.

1. Integrate the retry sequence into the existing FFmpeg proxy task.
2. Exercise the FFmpeg recipes with representative media, then check the native
   build and timeline on a desktop with NVIDIA hardware. Compare scrubbing
   with and without proxies, and verify export still uses original media.
