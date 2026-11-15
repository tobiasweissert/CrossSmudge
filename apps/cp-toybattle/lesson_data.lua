-- Original ToyBattleHowTo.cpp walkthrough positions, marks and captions.
return {nodes={
Goal={{500,30,'E',1,4,0},{30,250,' ',2,0,0},{500,250,' ',2,0,0},{970,250,' ',2,0,0},{215,560,' ',2,0,0},{785,560,' ',2,0,0},{500,790,' ',2,0,0},{500,965,'H',0,4,0}},
Turn={{500,30,'E',1,0,0},{30,250,' ',2,0,0},{500,250,' ',2,0,0},{970,250,' ',2,0,0},{215,560,' ',2,0,0},{785,560,' ',2,0,0},{500,790,'4',0,4,0},{500,965,'H',0,0,0}},
Reach={{500,30,'E',1,0,0},{30,250,' ',2,2,0},{500,250,' ',2,2,0},{970,250,' ',2,2,0},{215,560,' ',2,1,0},{785,560,' ',2,1,0},{500,790,'4',0,0,0},{500,965,'H',0,0,0}},
Cover={{500,30,'E',1,0,0},{30,250,' ',2,0,0},{500,250,'4',1,0,0},{970,250,' ',2,0,0},{215,560,'6',1,2,0},{785,560,'3',1,1,0},{500,790,'5',0,0,0},{500,965,'H',0,0,0}},
Chain={{500,30,'E',1,0,0},{30,250,' ',2,0,0},{500,250,'7',1,0,0},{970,250,'2',0,2,0},{215,560,'5',0,0,0},{785,560,'6',1,0,0},{500,790,'4',0,0,0},{500,965,'H',0,0,0}},
Region={{500,30,'E',1,0,0},{30,250,' ',2,0,0},{500,250,'6',0,0,0},{970,250,' ',2,0,0},{215,560,'5',0,0,0},{785,560,'3',0,0,0},{500,790,'4',0,0,0},{500,965,'H',0,0,0}},
Special={{500,30,'E',1,0,0},{30,250,' ',2,0,0},{500,250,' ',2,4,2},{970,250,' ',2,0,0},{215,560,' ',2,0,0},{785,560,' ',2,4,1},{500,790,' ',2,0,0},{500,965,'H',0,0,0}},
},links={{1,2},{1,3},{1,4},{2,3},{3,4},{2,5},{3,5},{3,6},{4,6},{5,6},{5,7},{6,7},{7,8}},pages={
{'THE GOAL','TAKE THE MEDALS THIS MAP IS WORTH, OR LAND ONE TROOP ON THEIR H.Q.','Goal'},
{'YOUR TURN','PLACE ONE TROOP NEXT TO YOUR H.Q., OR DRAW TWO. THAT IS THE WHOLE TURN.','Turn'},
{'YOUR REACH','THE NEXT ONE GOES BESIDE A BASE YOU ALREADY HOLD. NEVER FURTHER.','Reach'},
{'COVERING','YOU MAY LAND ON A SMALLER TROOP OF THEIRS. THE BURIED ONE STAYS BURIED.','Cover'},
{'THE LINE HOME','CUT THE WALK BACK TO YOUR H.Q. AND THE BASE AT THE FAR END STOPS COUNTING.','Chain'},
{'REGIONS','HOLD EVERY BASE AROUND A REGION AND ITS MEDALS ARE YOURS FOR GOOD.','Region'},
{'THE TROOPS','',false},
{'SPECIAL BASES','A BADGED BASE ACTS WHEN YOU LAND ON IT. TAP ? IN A GAME TO SEE WHICH ONES THIS MAP HAS.','Special'},
}}
