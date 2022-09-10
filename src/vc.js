import * as React from 'react';
import ReactDOM from 'react-dom';
import Slider from '@mui/material/Slider';
import axios from 'axios';
import Grid from '@mui/material/Grid'; // Grid version 1
import AppBar from '@mui/material/AppBar';
import Box from '@mui/material/Box';
import Toolbar from '@mui/material/Toolbar';
import Typography from '@mui/material/Typography';
import Card from '@mui/material/Card';
import Button from '@mui/material/Button';
import CardActions from '@mui/material/CardActions';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import StorageIcon from '@mui/icons-material/Storage';


class LiveImages extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      imageEndpoint: './get_live_image_jpg/'
    };
  }

  componentDidMount() {
    this.imgID = setInterval(() => {
      this.tickImg();
    }, 1000);
  }

  tickImg() {
    this.setState({imageEndpoint: './get_live_image_jpg/?' + Math.random()});
    console.log(this.state.imageEndpoint);
  }
  render() {
    return (
      <Card sx={{maxWidth: 1280}}>
        <CardMedia
          component="img"
          height="720"
          image={this.state.imageEndpoint}
          sx={{objectFit: 'contain'}}
        />
        <CardContent>
          <Typography gutterBottom variant="h5" component="div">
            CCTV
          </Typography>
        </CardContent>
        <CardActions>
        </CardActions>
      </Card>
    );
  }
}

class NavBar extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      user: null
    };
  }

  componentDidMount() {
  }

  render() {
    return (
      <Box sx={{flexGrow: 1, mb: '1rem'}}>
        <AppBar position="static">
          <Toolbar>
            <StorageIcon sx={{display: {md: 'flex'}, mr: 1}} />
            <Typography
              variant="h6"
              noWrap
              component="a"
              href="/"
              sx={{
                mr: 2,
                display: {md: 'flex'},
                fontFamily: 'monospace',
                fontWeight: 700,
                letterSpacing: '.1rem',
                color: 'inherit',
                textDecoration: 'none'
              }}
            >
              RACK MONITOR
            </Typography>
            <Typography variant="h6" component="div" sx={{flexGrow: 1}}>
            </Typography>
            <Button color="inherit">{this.state.user}</Button>
          </Toolbar>
        </AppBar>
      </Box>
    );
  }
}

class Index extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      currDate: new Date()
    };
  }

  render() {
    return (
      <>
        <NavBar />
        <div style={{maxWidth: '1280px', display: 'block', marginLeft: 'auto', marginRight: 'auto'}}>
          <Grid container>
            <Grid xs={12} md={7}>
              <Box display="flex" justifyContent="center" alignItems="center" mb='2rem'>
                <LiveImages />
              </Box>
            </Grid>
            <Grid xs={12} md={5} >
              <Box mb='2rem'>
              </Box>
            </Grid>
          </Grid>
        </div>
      </>
    );
  }
}

const container = document.getElementById('root');
ReactDOM.render(<Index />, container);
