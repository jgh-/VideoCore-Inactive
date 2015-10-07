/*

 Video Core
 Copyright (c) 2014 James G. Hurley

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

 */

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
        session.delegate = nil;
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
    
    
    @IBAction func btnFilterTouch(sender: AnyObject) {
        switch self.session.filter {
            
        case .Normal:
            self.session.filter = .Gray
        
        case .Gray:
            self.session.filter = .InvertColors
        
        case .InvertColors:
            self.session.filter = .Sepia
        
        case .Sepia:
            self.session.filter = .Fisheye
        
        case .Fisheye:
            self.session.filter = .Glow
        
        case .Glow:
            self.session.filter = .Normal
        
        default: // Future proofing
            break
        }
    }
}

