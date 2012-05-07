<?php
require_once('oauth-php/library/OAuthStore.php');
require_once('oauth-php/library/OAuthRequester.php');
require_once('oauth-php/library/OAuthRequestLogger.php');

DEFINE('OAUTH_LOG_REQUEST', true);

OAuthStore::instance('MySQL', array('server' => 'localhost', 'username' => 'root', 'password' => 'danjim101', 'database' => 'oauthconsumer'));

switch($_GET['action'])
{
    case 'AddServer':
        AddServerToOAuth();
        break;

    case 'ObtainAccess':
        ObtainAccessToAServer();
        break;

    case 'Exchange':
        ExchangeRequestForAccess();
        break;

    case 'Request':
        MakeSignedRequest();
        break;
}

function AddServerToOAuth()
{
    // Get the id of the current user (must be an int)
    $user_id = 1;
    $store = OAuthStore::instance();

    // The server description
    $server = array(
        'consumer_key' => '31662344be43f3818473abb099c8b8a204b81544d',
        'consumer_secret' => '344095a4a78fb59933a79464fb103325',
        'server_uri' => 'http://localhost/Series%201.1/111-server/server/services.php?service=rest',
        'signature_methods' => array('HMAC-SHA1', 'PLAINTEXT'),
        'request_token_uri' => 'http://localhost/Series%201.1/111-server/server/services.php?service=oauth&method=request_token',
        'authorize_uri' => 'http://localhost/Series%201.1/111-server/server/index.php?p=oauth&q=authorize',
        'access_token_uri' => 'http://localhost/Series%201.1/111-server/server/index.php?p=oauth&q=access_token'
    );

    // Save the server in the the OAuthStore
    $consumer_key = $store->updateServer($server, $user_id);
}

function ObtainAccessToAServer()
{
    $consumer_key = '31662344be43f3818473abb099c8b8a204b81544d';
    $user_id = 1;

    // Obtain a request token from the server
    try
    {
        $token = OAuthRequester::requestRequestToken($consumer_key, $user_id);
    }
    catch (OAuthException $e)
    {
        echo $e->getMessage();
        die('Failed');
    }

    // Callback to our (consumer) site, will be called when the user finished the authorization at the server
    $callback_uri = 'http://localhost/consumer/index.php?action=Exchange&consumer_key='.rawurlencode($consumer_key).'&usr_id='.intval($user_id);

    // Now redirect to the autorization uri and get us authorized
    if (!empty($token['authorize_uri']))
    {
        // Redirect to the server, add a callback to our server
        if (strpos($token['authorize_uri'], '?'))
        {
            $uri = $token['authorize_uri'] . '&';
        }
        else
        {
            $uri = $token['authorize_uri'] . '?';
        }
        $uri .= 'oauth_token='.rawurlencode($token['token']).'&oauth_callback='.rawurlencode($callback_uri);
    }
    else
    {
        // No authorization uri, assume we are authorized, exchange request token for access token
       $uri = $callback_uri . '&oauth_token='.rawurlencode($token['token']);
    }

    header('Location: '.$uri);
    exit();
}

function ExchangeRequestForAccess()
{
    // Request parameters are oauth_token, consumer_key and usr_id.
    $consumer_key = $_GET['consumer_key'];
    $oauth_token = $_GET['oauth_token'];
    $user_id = $_GET['usr_id'];

    try
    {
        OAuthRequester::requestAccessToken($consumer_key, $oauth_token, $user_id);
    }
    catch (OAuthException $e)
    {
        // Something wrong with the oauth_token.
        // Could be:
        // 1. Was already ok
        // 2. We were not authorized
        die($e->getMessage());
    }
    
    echo 'Authorization Given. <a href="index.php?action=Request">Click to make a signed request</a>.';
}

function MakeSignedRequest()
{
    // The request uri being called.
    $user_id = 1;
    $request_uri = 'http://localhost/Series%201.1/111-server/server/services.php';

    // Parameters, appended to the request depending on the request method.
    // Will become the POST body or the GET query string.
    $params = array(
               'service' => 'rest',
               'method' => 'version',
               'response' => 'json'
         );

    // Obtain a request object for the request we want to make
    $req = new OAuthRequester($request_uri, 'GET', $params);

    // Sign the request, perform a curl request and return the results, throws OAuthException exception on an error
    $result = $req->doRequest($user_id);

    // $result is an array of the form: array ('code'=>int, 'headers'=>array(), 'body'=>string)
    var_dump($result['body']);
}
?>
