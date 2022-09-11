import * as React from 'react';
import ReactDOM from 'react-dom';
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
import Select, { SelectChangeEvent } from '@mui/material/Select';
import MenuItem from '@mui/material/MenuItem';
import InputLabel from '@mui/material/InputLabel';
import GasMeterIcon from '@mui/icons-material/GasMeter';
import FormControl from '@mui/material/FormControl';
import Stack from '@mui/material/Stack';

class LiveImages extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      imageEndpoint: './get_live_image_jpg/',
      valveSessionLengthSec: 10,
      simpleSnackbar: null
    };
    this.onSelectItemChange = this.onSelectItemChange.bind(this);
    this.onOpenValveButtonClick = this.onOpenValveButtonClick.bind(this);
  }

  componentDidMount() {
    this.imgID = setInterval(() => {
      this.tickImg();
    }, 600);
  }

  tickImg() {
    this.setState({imageEndpoint: './get_live_image_jpg/?' + Math.random()});
  }

  onSelectItemChange(event) {
    const parsedValue = parseInt(event.target.value);
    this.setState({
      valveSessionLengthSec: parsedValue
    });
  }

  onOpenValveButtonClick() {
    axios.get(`./open_valve/?length_sec=${this.state.valveSessionLengthSec}`)
        .then((response) => {
          console.log(response);
        })
        .catch((error) => {
          console.log(error);
          alert(`${error}`);
        });
  }

  render() {
    return (
      <>
        <Card sx={{maxWidth: 1280}}>
          <CardMedia
            component="img"
            height="530"
            image={this.state.imageEndpoint}
            sx={{objectFit: 'contain'}}
          />
          <CardContent>
            <Box sx={{minWidth: 120}}>
              <Stack spacing={2} direction="row">
                <FormControl fullWidth>
                  <InputLabel id="select-duration-label">时长</InputLabel>
                  <Select
                    labelId="select-duration-label" id="simple-duration" value={this.state.valveSessionLengthSec}
                    label="时长" onChange={this.onSelectItemChange}
                  >
                    <MenuItem value={10}>10秒</MenuItem>
                    <MenuItem value={30}>30秒</MenuItem>
                    <MenuItem value={60}>60秒</MenuItem>
                  </Select>
                </FormControl>
                <Button size="small" color="primary" onClick={this.onOpenValveButtonClick}>
                  开阀
                </Button>
              </Stack>
            </Box>
          </CardContent>
          <CardActions>
          </CardActions>
        </Card>
      </>
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
            <GasMeterIcon sx={{display: {md: 'flex'}, mr: 1}} />
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
              水阀控制
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
        <div style={{maxWidth: '1440px', display: 'block', marginLeft: 'auto', marginRight: 'auto'}}>
          <Grid container>
            <Grid xs={12} md={8}>
              <Box display="flex" justifyContent="center" alignItems="center" mb='2rem'>
                <LiveImages />
              </Box>
            </Grid>
            <Grid xs={12} md={4} >
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
