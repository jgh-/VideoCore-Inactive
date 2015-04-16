//
//  ViewController.swift
//  SampleBroadcaster-Swift
//
//  Created by Josh Lieberman on 4/11/15.
//  Copyright (c) 2015 videocore. All rights reserved.
//

import UIKit

class ViewController: UIViewController, VCSessionDelegate
{

    @IBOutlet weak var previewView: UIView!
    @IBOutlet weak var btnConnect: UIButton!
    
    var session:VCSimpleSession = VCSimpleSession(videoSize: CGSize(width: 1280, height: 720), frameRate: 30, bitrate: 1000000, useInterfaceOrientation: false)
    
    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view, typically from a nib.
        
        previewView.addSubview(session.previewView)
        session.previewView.frame = previewView.bounds
        session.delegate = self
        
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    
    deinit {
        btnConnect = nil
        previewView = nil
    }

    @IBAction func btnConnectTouch(sender: AnyObject) {
        switch session.rtmpSessionState {
        case .None, .PreviewStarted, .Ended, .Error:
            session.startRtmpSessionWithURL("rtmp://192.168.1.151/live", andStreamKey: "myStream")
        default:
            session.endRtmpSession()
            break
        }
    }
    
    func connectionStatusChanged(sessionState: VCSessionState) {
        switch session.rtmpSessionState {
        case .Starting:
            btnConnect.setTitle("Connecting", forState: .Normal)
            
        case .Started:
            btnConnect.setTitle("Disconnect", forState: .Normal)
            
        default:
            btnConnect.setTitle("Connect", forState: .Normal)
        }
    }
}

