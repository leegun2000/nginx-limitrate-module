# nginx-limitrate-module
example configuration
```
http {
    server {
        perl_set $get_media_burst_length 'sub {
           my $r = shift;
           my $args = $r->args;
           my $content_burstsize = 0;
           if ($arg =~ s/vod=1080//) {
                $content_burstsize = ((5120 * 1024) / 8) * 5;
           }
 
           return int($content_burstsize);
        }';
        set $limit 1024k;
        location / {
            limit_rate_var $limit;
            limit_rate_after_var $get_media_burst_length;
            proxy_pass         http://127.0.0.1:8000;           
        }
    }
}
```
