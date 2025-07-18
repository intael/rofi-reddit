# Rofi plugin template

Template for creating rofi plugins.

To run this you need an up to date checkout of rofi git installed.

Run rofi like:

```bash
rofi -show rofi_reddit -modi rofi_reddit
```

## Reddit app stuff

How to create app: https://business.reddithelp.com/helpcenter/s/article/Create-a-Reddit-Application
Your app: https://www.reddit.com/prefs/apps

### Reddit api

Create the app and get the access token like this:
```shell
curl --request POST \
  --url https://www.reddit.com/api/v1/access_token \
  -A '<client-name>' \
  -u '<client-id>:<client-secret>' \
  --header 'content-type: application/x-www-form-urlencoded' \
  --data scope=read \
  --data grant_type=client_credentials
```

Request with auth bearer
```shell
curl --request GET \
  --url https://oauth.reddit.com/r/python/hot \
  --header 'Authorization: Bearer <access-token>'
`
